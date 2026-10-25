// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "sync.h"
#include "caldav.h"
#include "data.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "task-item.h"
#include "task-list-item.h"
#include "task-list.h"
#include "utils.h"
#include "window.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libical-glib/libical-glib.h>
#include <libical/ical.h>

static inline bool ensure_client_connected(void);
static void obj_push_idle_cb(gpointer user_data);

// ---------- GLOBAL ---------- //

static gboolean sync_in_progress = false;
static CalDAVClient *client = NULL;
static GPtrArray *objects_to_push = NULL;

typedef struct {
  enum {
    CAL_ADDED,
    CAL_MODIFIED,
    CAL_REMOVED,
    TASK_ADDED,
    TASK_MODIFIED,
    TASK_REMOVED,
  } op;
  CalDAVCalendar *calendar;
  icalcomponent *ical;
  gchar *name;
  gchar *color;
  gchar *uid;
} _CBData;

static void cb_data_free(_CBData *data) {
  g_clear_pointer(&data->name, g_free);
  g_clear_pointer(&data->color, g_free);
  g_clear_pointer(&data->uid, g_free);
  g_clear_pointer(&data, g_free);
}

typedef struct {
  enum PushObjOP {
    PUSH_OBJ_LIST_CREATE,
    PUSH_OBJ_LIST_UPDATE,
    PUSH_OBJ_LIST_DELETE,
    PUSH_OBJ_TASK_PUSH,
    PUSH_OBJ_TASK_DELETE,
  } op;
  gchar *uid; // !!! MUST BE SET !!!
  gchar *name;
  gchar *color;

  ErrandsTaskListItem *list;
  ErrandsTaskItem *task;
  icalcomponent *ical;

  gboolean pushed;
  gint count;
} PushObj;

static inline PushObj *push_obj_new(enum PushObjOP op, const char *uid) {
  g_assert_nonnull(uid);

  PushObj *o = g_new0(PushObj, 1);
  o->op = op;
  o->uid = g_strdup(uid);
  return o;
}

static inline PushObj *objects_to_push_has(const char *uid, enum PushObjOP op) {
  g_assert_nonnull(uid);
  g_assert_nonnull(objects_to_push);

  for_range(i, 0, objects_to_push->len) {
    PushObj *o = g_ptr_array_index(objects_to_push, i);
    if (g_str_equal(uid, o->uid) && o->op == op) return o;
  }
  return FALSE;
}

static void obj_push(PushObj *o) {
  g_assert_nonnull(o);
  g_assert_nonnull(o->uid);

  if (!ensure_client_connected()) return;

  switch (o->op) {
  case PUSH_OBJ_LIST_CREATE: {
    // List already exist - just update it
    CalDAVCalendar *calendar = caldav_client_find_calendar(client, o->uid);
    if (calendar) {
      o->op = PUSH_OBJ_LIST_UPDATE;
      obj_push(o);
      return;
    }
    g_message("Sync: Creating list on remote: %s", o->uid);
    CalDAVCalendar *cal = caldav_client_create_calendar(client, o->uid, o->name, o->color);
    o->pushed = cal != NULL;
  } break;

  case PUSH_OBJ_LIST_UPDATE: {
    CalDAVCalendar *c = caldav_client_find_calendar(client, o->uid);
    if (!c) return;
    g_message("Sync: Updating list on remote: %s", o->uid);
    o->pushed = caldav_calendar_set_properties(c, o->name, o->color);
  } break;

  case PUSH_OBJ_LIST_DELETE: {
    CalDAVCalendar *c = caldav_client_find_calendar(client, o->uid);
    if (!c) return;
    g_message("Sync: Deleting list on remote: %s", o->uid);
    o->pushed = caldav_calendar_remove(c);
  } break;

  case PUSH_OBJ_TASK_PUSH: {
    CalDAVCalendar *c = caldav_client_find_calendar(client, o->list->uid);
    if (!c) return;
    g_message("Sync: Creating/Updating task '%s' in remote calendar '%s'", o->uid, o->list->uid);
    g_autoptr(ICalComponent) wrapped = i_cal_object_construct(I_CAL_TYPE_COMPONENT, o->ical, NULL, FALSE, NULL);
    o->pushed = caldav_calendar_push_task(c, wrapped);
  } break;

  case PUSH_OBJ_TASK_DELETE: {
    CalDAVCalendar *c = caldav_client_find_calendar(client, o->list->uid);
    if (!c) return;
    g_message("Sync: Deleting task '%s' from remote list '%s'", o->uid, o->list->uid);
    o->pushed = caldav_calendar_remove_task(c, o->uid);
  } break;
  }
  g_idle_add_once(obj_push_idle_cb, o);
}

static inline void obj_free(PushObj *o) {
  if (!o) return;
  if (o->list) g_object_unref(o->list);
  if (o->task) g_object_unref(o->task);
  g_clear_pointer(&o->ical, icalcomponent_free);
  g_clear_pointer(&o->uid, g_free);
  g_clear_pointer(&o->name, g_free);
  g_clear_pointer(&o->color, g_free);
}

// ---------- HELPERS ---------- //

static inline ErrandsTaskListItem *find_local_task_list(const char *uid) {
  g_assert_nonnull(uid);

  GListModel *model = G_LIST_MODEL(task_lists_model);
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskListItem) item = g_list_model_get_item(model, i);
    if (g_str_equal(item->uid, uid)) return item;
  }
  return NULL;
}

static inline icalcomponent *find_local_task(ErrandsTaskListItem *list, const char *uid) {
  g_assert_nonnull(list);
  g_assert_nonnull(uid);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(list));

  g_autoptr(GPtrArray) tasks = vcalendar_to_array(list->ical);
  for_range(i, 0, tasks->len) {
    icalcomponent *task = g_ptr_array_index(tasks, i);
    const char *task_uid = errands_data_get_uid(task);
    if (g_str_equal(uid, task_uid)) return task;
  }
  return NULL;
}

// Search model recursively and find `ErrandsTaskItem` with matching `uid`
static inline ErrandsTaskItem *find_task_item(GListModel *model, const char *uid) {
  g_assert_nonnull(model);
  g_assert_nonnull(uid);

  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    if (g_str_equal(uid, errands_task_item_get_uid(item))) return item;
    ErrandsTaskItem *child = find_task_item(errands_task_item_get_children_model(item), uid);
    if (child) return child;
  }
  return NULL;
}

static inline void collect_task_items(GListModel *model, GPtrArray *array) {
  g_assert_nonnull(model);
  g_assert_nonnull(array);

  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) task = g_list_model_get_item(model, i);
    g_ptr_array_add(array, task);
    collect_task_items(errands_task_item_get_children_model(task), array);
  }
}

static inline bool merge_tasks(icalcomponent *dest, icalcomponent *source) {
  if (!source || !dest) return FALSE;

  icaltimetype dest_changed = errands_data_get_changed(dest);
  icaltimetype source_changed = errands_data_get_changed(source);
  if (icaltime_compare(dest_changed, source_changed) <= 0) return false;

  char *dest_str = icalcomponent_as_ical_string(dest);
  char *source_str = icalcomponent_as_ical_string(source);
  if (g_str_equal(dest_str, source_str)) return false;

  g_autoptr(GPtrArray) props = g_ptr_array_new();
  for (icalproperty *prop = icalcomponent_get_first_property(source, ICAL_ANY_PROPERTY); prop;
       prop = icalcomponent_get_next_property(source, ICAL_ANY_PROPERTY)) {
    g_ptr_array_add(props, prop);
  }
  for (guint i = 0; i < props->len; i++) {
    icalproperty *src = g_ptr_array_index(props, i);
    icalproperty_kind kind = icalproperty_isa(src);
    icalproperty *dst = icalcomponent_get_first_property(dest, kind);
    if (dst) {
      const char *src_str = icalproperty_as_ical_string(src);
      const char *dst_str = icalproperty_as_ical_string(dst);
      if (g_strcmp0(src_str, dst_str) == 0) continue;
      icalcomponent_remove_property(dest, dst);
    }
    icalcomponent_add_property(dest, icalproperty_clone(src));
  }

  return true;
}

// ---------- IDLE CALLBACKS ---------- //

static gboolean remote_event_idle_cb(gpointer user_data) {
  g_assert_nonnull(user_data);

  const _CBData *data = user_data;
  switch (data->op) {

  case CAL_ADDED: {
    const char *uid = caldav_calendar_get_uid(data->calendar);
    if (find_local_task_list(uid) != NULL) return G_SOURCE_REMOVE;
    g_message("Sync: Remote list added: %s", uid);
    const char *name = caldav_calendar_get_name(data->calendar);
    const char *color = caldav_calendar_get_color(data->calendar);
    g_autoptr(ErrandsTaskListItem) item = errands_task_list_item_create(uid, name, color, false, true);
    g_list_store_append(task_lists_model, item);
    errands_task_list_item_save(item);
    errands_sidebar_update_filter_rows();
    errands_window_add_toast(tmp_str_printf(_("Added remote task list: %s"), name), 5);
  } break;

  case CAL_MODIFIED: {
    const char *uid = caldav_calendar_get_uid(data->calendar);
    ErrandsTaskListItem *list = find_local_task_list(uid);
    if (!list) {
      g_warning("Sync: Couldn't find local list: %s", uid);
      return G_SOURCE_REMOVE;
    }
    // Local list was modified first
    if (!errands_data_get_synced(list->ical)) return G_SOURCE_REMOVE;
    // TODO: lock sync
    if (data->name && !g_str_equal(data->name, list->title)) {
      errands_task_list_item_set_title(list, data->name);
      errands_task_list_update(state.main_window->task_list);
    }
    if (data->color) {
      const char *color = gdk_rgba_to_str(&list->color);
      if (!g_str_equal(data->color, color)) errands_task_list_item_set_color_from_string(list, data->color);
    }
    g_message("Sync: Calendar properties changed on remote: %s", uid);
  } break;

  case CAL_REMOVED: {
    g_message("Sync: Calendar removed on remote: %s", data->uid);
    ErrandsTaskListItem *list = find_local_task_list(data->uid);
    if (!list) {
      g_warning("Sync: Couldn't find local list: %s", data->uid);
      return G_SOURCE_REMOVE;
    }
    errands_window_add_toast(tmp_str_printf(_("Removed remote task list: %s"), list->title), 5);
    errands_task_list_item_delete(list);
  } break;

  case TASK_ADDED: {
    ErrandsTaskListItem *list = find_local_task_list(caldav_calendar_get_uid(data->calendar));
    if (find_local_task(list, errands_data_get_uid(data->ical))) return G_SOURCE_REMOVE;
    icalcomponent_add_component(list->ical, data->ical);
    const char *parent = errands_data_get_parent(data->ical);
    errands_data_set_synced(data->ical, true);
    ErrandsTaskItem *task_item = NULL;
    if (!parent) {
      task_item = errands_task_item_new(data->ical, list, NULL);
      g_list_store_append(list->tasks, task_item);
    } else {
      ErrandsTaskItem *parent_task_item = find_task_item(G_LIST_MODEL(list->tasks), parent);
      if (!parent_task_item) return G_SOURCE_REMOVE;
      GListModel *child_model = errands_task_item_get_children_model(parent_task_item);
      task_item = errands_task_item_new(data->ical, list, parent_task_item);
      g_list_store_append(G_LIST_STORE(child_model), task_item);
    }

    icalcomponent *ical = errands_task_item_get_ical(task_item);
    errands_data_set_synced(ical, true);
    errands_task_list_item_save(list);
    errands_task_list_item_update_count(list);
    errands_sidebar_update_filter_rows();
    errands_task_list_update(state.main_window->task_list);

    g_message("Sync: Task added to list '%s': '%s'", list->uid, errands_data_get_uid(data->ical));
  } break;

  case TASK_MODIFIED: {
    const char *list_uid = caldav_calendar_get_uid(data->calendar);
    icalcomponent *task_ical = data->ical;
    ErrandsTaskListItem *list = find_local_task_list(list_uid);
    if (!list) {
      g_warning("Sync: Failed to find local list: %s", list_uid);
      return G_SOURCE_REMOVE;
    }
    const char *uid = errands_data_get_uid(task_ical);
    ErrandsTaskItem *task = find_task_item(G_LIST_MODEL(list->tasks), uid);
    if (!task) {
      g_warning("Sync: Failed to find local task: %s", uid);
      return G_SOURCE_REMOVE;
    }
    icalcomponent *ical = errands_task_item_get_ical(task);
    bool changed = merge_tasks(ical, task_ical);
    if (!changed) break;
    g_message("Sync: Task modified: %s", uid);
    errands_task_item_set_ical(task, ical);
    errands_data_set_synced(ical, true);
    errands_task_list_item_save(list);
  } break;

  case TASK_REMOVED: {
    const gchar *list_uid = caldav_calendar_get_uid(data->calendar);
    ErrandsTaskListItem *list = find_local_task_list(list_uid);
    if (!list) break;
    ErrandsTaskItem *task = find_task_item(G_LIST_MODEL(list->tasks), data->uid);
    if (!task) break;
    g_message("Sync: Task '%s' removed from remote list '%s'", data->uid, list_uid);
    errands_task_item_delete(task);
    icalcomponent *ical = errands_task_item_get_ical(task);
    errands_data_set_synced(ical, true);
    errands_task_list_item_save(list);
    errands_task_list_item_update_count(list);
    errands_sidebar_update_filter_rows();
    errands_task_list_update(state.main_window->task_list);
  } break;
  }

  return G_SOURCE_REMOVE;
}

static void obj_push_idle_cb(gpointer user_data) {
  PushObj *o = user_data;

  switch (o->op) {
  case PUSH_OBJ_LIST_CREATE: {
    if (!o->pushed) {
      g_warning("Sync: Creating remote list failed: %s", o->list->uid);
      errands_window_add_toast(tmp_str_printf(_("Creating remote list failed. Limits may be reached.")), 5);
    } else g_message("Sync: Created remote list: %s", o->list->uid);
    errands_data_set_synced(o->list->ical, o->pushed);
    errands_task_list_item_save(o->list);
    if (!o->pushed) break;
    g_autoptr(GPtrArray) tasks = g_ptr_array_new();
    collect_task_items(G_LIST_MODEL(o->list->tasks), tasks);
    for_range(i, 0, tasks->len) {
      ErrandsTaskItem *task = g_ptr_array_index(tasks, i);
      errands_sync_push_task(task);
    }
  } break;

  case PUSH_OBJ_LIST_UPDATE: {
    if (!o->pushed) g_warning("Sync: Updating remote list failed: %s", o->list->uid);
    else g_message("Sync: Updated remote list: %s", o->list->uid);
    errands_data_set_synced(o->list->ical, o->pushed);
    errands_task_list_item_save(o->list);
  } break;

  case PUSH_OBJ_LIST_DELETE: {
    if (!o->pushed) g_warning("Sync: Deleting remote list failed: %s", o->list->uid);
    else g_message("Sync: Deleted remote list: %s", o->list->uid);
    errands_data_set_synced(o->list->ical, o->pushed);
    errands_task_list_item_save(o->list);
  } break;

  case PUSH_OBJ_TASK_PUSH: {
    if (!o->pushed) g_warning("Sync: Creating/Updating remote task failed: %s", errands_task_item_get_uid(o->task));
    else g_message("Sync: Created/Updated remote task: %s", errands_task_item_get_uid(o->task));
    icalcomponent *ical = errands_task_item_get_ical(o->task);
    errands_data_set_synced(ical, o->pushed);
    errands_task_list_item_save(errands_task_item_get_list(o->task));
    if (!o->pushed) break;
    g_autoptr(GPtrArray) tasks = g_ptr_array_new();
    collect_task_items(errands_task_item_get_children_model(o->task), tasks);
    for_range(i, 0, tasks->len) {
      ErrandsTaskItem *item = g_ptr_array_index(tasks, i);
      errands_sync_push_task(item);
    }
  } break;

  case PUSH_OBJ_TASK_DELETE: {
    if (!o->pushed) g_warning("Sync: Deleting remote task failed: %s", o->uid);
    else g_message("Sync: Deleted remote task: %s", o->uid);
    icalcomponent *ical = errands_task_item_get_ical(o->task);
    errands_data_set_synced(ical, o->pushed);
    ErrandsTaskListItem *list = errands_task_item_get_list(o->task);
    errands_task_list_item_save(list);
    errands_task_list_item_update_count(list);
    errands_sidebar_update_filter_rows();
    errands_task_list_update(state.main_window->task_list);
  } break;
  }

  if (--o->count <= 0) g_ptr_array_remove(objects_to_push, o);
}

// ---------- CALDAV CALLBACKS ---------- //

static void cal_added_cb(CalDAVCalendar *c) {
  g_assert_nonnull(c);

  _CBData *data = g_new0(_CBData, 1);
  data->op = CAL_ADDED;
  data->calendar = c;
  g_idle_add_full(G_PRIORITY_DEFAULT, remote_event_idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void cal_modified_cb(CalDAVCalendar *c, const char *name, const char *color) {
  g_assert_nonnull(c);

  _CBData *data = g_new0(_CBData, 1);
  data->op = CAL_MODIFIED;
  data->calendar = c;
  data->name = name ? g_strdup(name) : NULL;
  data->color = color ? g_strdup(color) : NULL;
  g_idle_add_full(G_PRIORITY_DEFAULT, remote_event_idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void cal_removed_cb(const char *uid) {
  g_assert_nonnull(uid);

  _CBData *data = g_new0(_CBData, 1);
  data->op = CAL_REMOVED;
  data->uid = g_strdup(uid);
  g_idle_add_full(G_PRIORITY_DEFAULT, remote_event_idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void task_added_cb(CalDAVCalendar *c, ICalComponent *ical) {
  g_assert_nonnull(c);
  g_assert_nonnull(ical);

  icalcomponent *task = i_cal_object_get_native(I_CAL_OBJECT(ical));
  if (icalcomponent_isa(task) != ICAL_VTODO_COMPONENT) return;

  _CBData *data = g_new0(_CBData, 1);
  data->op = TASK_ADDED;
  data->calendar = c;
  data->ical = icalcomponent_clone(task);
  g_idle_add_full(G_PRIORITY_DEFAULT, remote_event_idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void task_modified_cb(CalDAVCalendar *c, ICalComponent *ical) {
  g_assert_nonnull(c);
  g_assert_nonnull(ical);

  icalcomponent *task = i_cal_object_get_native(I_CAL_OBJECT(ical));
  if (icalcomponent_isa(task) != ICAL_VTODO_COMPONENT) return;

  _CBData *data = g_new0(_CBData, 1);
  data->op = TASK_MODIFIED;
  data->calendar = c;
  data->ical = icalcomponent_clone(task);
  g_idle_add_full(G_PRIORITY_DEFAULT, remote_event_idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void task_removed_cb(CalDAVCalendar *c, const char *uid) {
  g_assert_nonnull(c);
  g_assert_nonnull(uid);

  _CBData *data = g_new0(_CBData, 1);
  data->op = TASK_REMOVED;
  data->calendar = c;
  data->uid = g_strdup(uid);
  g_idle_add_full(G_PRIORITY_DEFAULT, remote_event_idle_cb, data, (GDestroyNotify)cb_data_free);
}

// ---------- WORKER THREADS CALLBACKS ---------- //

static void sync_thread_cb(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  if (!ensure_client_connected()) return;
  for_range(i, 0, objects_to_push->len) {
    PushObj *o = g_ptr_array_index(objects_to_push, i);
    obj_push(o);
  }
  caldav_client_poll(client);
}

static void sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  sync_in_progress = FALSE;
  errands_sidebar_toggle_sync_indicator(FALSE);
  g_message("Sync: Finished");
}

// ---------- PRIVATE FUNCTIONS ---------- //

static inline bool ensure_client_connected(void) {
  if (!errands_settings_get_sync_enabled(state.settings)) return false;
  if (!client) {
    CalDAVClientConfig config = {
        .url = errands_settings_get_sync_url(state.settings),
        .username = errands_settings_get_sync_username(state.settings),
        .password = errands_settings_get_sync_password(state.settings),
        .cal_added_cb = cal_added_cb,
        .cal_modified_cb = cal_modified_cb,
        .cal_removed_cb = cal_removed_cb,
        .task_added_cb = task_added_cb,
        .task_modified_cb = task_modified_cb,
        .task_removed_cb = task_removed_cb,
    };
    client = caldav_client_new(config);
  }
  if (!client) return false;
  return client != NULL;
}

// ---------- PUBLIC ---------- //

void errands_sync_init(void) { objects_to_push = g_ptr_array_new_with_free_func((GDestroyNotify)obj_free); }

void errands_sync_cleanup(void) {
  g_message("Sync: Cleanup");
  g_clear_pointer(&objects_to_push, g_ptr_array_unref);
  g_clear_pointer(&client, caldav_client_free);
}

gboolean errands_sync(void) {
  if (!errands_settings_get_sync_enabled(state.settings)) return G_SOURCE_CONTINUE;
  g_message("Sync: Start");
  if (sync_in_progress) {
    g_message("Sync: Already in progress");
    return G_SOURCE_CONTINUE;
  }
  sync_in_progress = true;
  errands_sidebar_toggle_sync_indicator(TRUE);
  run_in_thread(sync_thread_cb, NULL, sync_finished_cb, NULL);
  return G_SOURCE_CONTINUE;
}

void errands_sync_create_list(ErrandsTaskListItem *list) {
  g_assert_nonnull(list);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(list));

  if (!errands_settings_get_sync_enabled(state.settings)) return;

  PushObj *o = objects_to_push_has(list->uid, PUSH_OBJ_LIST_CREATE);
  if (o) {
    o->count++;
    return;
  }

  o = push_obj_new(PUSH_OBJ_LIST_CREATE, list->uid);
  o->list = g_object_ref(list);
  o->name = g_strdup(list->title);
  o->color = g_strdup(gdk_rgba_to_str(&list->color));
  g_ptr_array_add(objects_to_push, o);
}

void errands_sync_update_list(ErrandsTaskListItem *list) {
  g_assert_nonnull(list);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(list));

  if (!errands_settings_get_sync_enabled(state.settings)) return;

  PushObj *o = objects_to_push_has(list->uid, PUSH_OBJ_LIST_UPDATE);
  if (o) {
    o->count++;
    return;
  }

  o = push_obj_new(PUSH_OBJ_LIST_UPDATE, list->uid);
  o->list = g_object_ref(list);
  o->name = g_strdup(list->title);
  o->color = g_strdup(gdk_rgba_to_str(&list->color));
  g_ptr_array_add(objects_to_push, o);
}

void errands_sync_delete_list(ErrandsTaskListItem *list) {
  g_assert_nonnull(list);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(list));

  if (!errands_settings_get_sync_enabled(state.settings)) return;

  PushObj *o = objects_to_push_has(list->uid, PUSH_OBJ_LIST_DELETE);
  if (o) {
    o->count++;
    return;
  }

  o = push_obj_new(PUSH_OBJ_LIST_DELETE, list->uid);
  o->list = g_object_ref(list);
  g_ptr_array_add(objects_to_push, o);
}

void errands_sync_push_task(ErrandsTaskItem *task) {
  g_assert_nonnull(task);
  g_assert(ERRANDS_IS_TASK_ITEM(task));

  if (!errands_settings_get_sync_enabled(state.settings)) return;

  const char *uid = errands_task_item_get_uid(task);
  PushObj *o = objects_to_push_has(uid, PUSH_OBJ_TASK_PUSH);
  if (o) {
    o->count++;
    return;
  }

  o = push_obj_new(PUSH_OBJ_TASK_PUSH, uid);
  o->list = g_object_ref(errands_task_item_get_list(task));
  o->task = g_object_ref(task);
  o->ical = icalcomponent_clone(errands_task_item_get_ical(task));
  g_ptr_array_add(objects_to_push, o);
}

void errands_sync_delete_task(ErrandsTaskItem *task) {
  g_assert_nonnull(task);
  g_assert(ERRANDS_IS_TASK_ITEM(task));

  if (!errands_settings_get_sync_enabled(state.settings)) return;

  const char *uid = errands_task_item_get_uid(task);
  PushObj *o = objects_to_push_has(uid, PUSH_OBJ_TASK_DELETE);
  if (o) {
    o->count++;
    return;
  }

  o = push_obj_new(PUSH_OBJ_TASK_DELETE, uid);
  o->list = g_object_ref(errands_task_item_get_list(task));
  o->task = g_object_ref(task);
  g_ptr_array_add(objects_to_push, o);
}

void errands_sync_move_task(ErrandsTaskItem *task, ErrandsTaskListItem *old_list) {
  ErrandsTaskListItem *new_list = errands_task_item_get_list(task);
  if (old_list == new_list) {
    errands_sync_push_task(task);
    return;
  }
  errands_sync_push_task(task);
  errands_sync_delete_task(task);
}
