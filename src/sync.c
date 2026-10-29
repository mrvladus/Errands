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

static inline void pushed_objs_store(const char *uid);
static inline bool pushed_objs_is_stored(const char *uid);
static inline bool ensure_client_connected(void);

static void obj_pushed_idle_cb(gpointer user_data);

// ---------- GLOBAL ---------- //

static gboolean sync_in_progress = false;
static CalDAVClient *client = NULL;
static GPtrArray *objects_to_push = NULL;
static GHashTable *pushed_objs = NULL;

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
    PUSH_OBJ_LIST_PUSH,
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

static inline void obj_free(PushObj *o) {
  if (!o) return;
  g_clear_object(&o->list);
  g_clear_object(&o->task);
  g_clear_pointer(&o->ical, icalcomponent_free);
  g_clear_pointer(&o->uid, g_free);
  g_clear_pointer(&o->name, g_free);
  g_clear_pointer(&o->color, g_free);
  g_clear_pointer(&o, g_free);
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

  switch (o->op) {
  case PUSH_OBJ_LIST_PUSH: {
    g_assert_nonnull(o->list);
    g_assert_nonnull(o->list->uid);
    g_assert_nonnull(o->list->title);

    CalDAVCalendar *c = caldav_client_find_calendar(client, o->list->uid);
    if (!c) {
      g_message("Sync: Creating list on remote: %s", o->list->uid);
      CalDAVCalendar *cal = caldav_client_create_calendar(client, o->list->uid, o->list->title, gdk_rgba_to_str(&o->list->color));
      o->pushed = cal != NULL;
      break;
    }
    g_message("Sync: Updating list on remote: %s", o->list->uid);
    o->pushed = caldav_calendar_set_properties(c, o->list->title, gdk_rgba_to_str(&o->list->color));
    if (o->pushed) pushed_objs_store(o->list->uid);
  } break;

  case PUSH_OBJ_LIST_DELETE: {
    CalDAVCalendar *c = caldav_client_find_calendar(client, o->uid);
    if (!c) break;
    g_message("Sync: Deleting list on remote: %s", o->uid);
    o->pushed = caldav_calendar_remove(c);
  } break;

  case PUSH_OBJ_TASK_PUSH: {
    CalDAVCalendar *c = caldav_client_find_calendar(client, o->list->uid);
    g_message("Sync: Push task '%s' to remote calendar '%s'", o->uid, o->list->uid);
    if (!c) {
      g_message("Sync: List '%s' not found on remote. Creating.", o->list->uid);
      PushObj *cal = push_obj_new(PUSH_OBJ_LIST_PUSH, o->list->uid);
      cal->list = g_object_ref(o->list);
      obj_push(cal);
      break;
    }
    c = caldav_client_find_calendar(client, o->list->uid);
    if (!c) break;
    g_autoptr(ICalComponent) wrapped = i_cal_object_construct(I_CAL_TYPE_COMPONENT, o->ical, NULL, FALSE, NULL);
    o->pushed = caldav_calendar_push_task(c, wrapped);
    if (o->pushed) pushed_objs_store(o->uid);
  } break;

  case PUSH_OBJ_TASK_DELETE: {
    CalDAVCalendar *c = caldav_client_find_calendar(client, o->list->uid);
    if (!c) break;
    g_message("Sync: Deleting task '%s' from remote list '%s'", o->uid, o->list->uid);
    o->pushed = caldav_calendar_remove_task(c, o->uid);
  } break;
  }
  g_idle_add_once(obj_pushed_idle_cb, o);
}

// ---------- HELPERS ---------- //

static inline void pushed_objs_store(const char *uid) { g_hash_table_add(pushed_objs, g_strdup(uid)); }

static inline bool pushed_objs_is_stored(const char *uid) { return pushed_objs && g_hash_table_contains(pushed_objs, uid); }

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
  if (!list || !uid) return NULL;

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

static inline bool merge_tasks(icalcomponent *old, icalcomponent *new) {
  if (!old || !new) return false;

  icaltimetype old_changed = errands_data_get_changed(old);
  icaltimetype new_changed = errands_data_get_changed(new);
  if (!icaltime_is_valid_time(new_changed) || icaltime_is_null_time(new_changed)) return false;
  if (icaltime_compare(new_changed, old_changed) <= 0) return false;

  // Snapshot new's properties so we can safely mutate old
  g_autoptr(GPtrArray) props = g_ptr_array_new();
  for (icalproperty *prop = icalcomponent_get_first_property(new, ICAL_ANY_PROPERTY); prop;
       prop = icalcomponent_get_next_property(new, ICAL_ANY_PROPERTY)) {
    g_ptr_array_add(props, prop);
  }
  // Group by kind: for each kind present in new, remove ALL old ones, then add all of new's.
  // This avoids duplicates for multi-valued props.
  GHashTable *by_kind = g_hash_table_new(g_direct_hash, g_direct_equal);
  for (guint i = 0; i < props->len; i++) {
    icalproperty *src = g_ptr_array_index(props, i);
    icalproperty_kind kind = icalproperty_isa(src);
    // Never overwrite identity-bearing properties
    if (kind == ICAL_UID_PROPERTY || kind == ICAL_RECURRENCEID_PROPERTY) continue;
    g_hash_table_insert(by_kind, GINT_TO_POINTER(kind), src);
  }

  GHashTableIter it;
  gpointer key, value;
  g_hash_table_iter_init(&it, by_kind);
  while (g_hash_table_iter_next(&it, &key, &value)) {
    icalproperty_kind kind = (icalproperty_kind)GPOINTER_TO_INT(key);
    // Remove *all* existing properties of this kind
    icalproperty *dst;
    while ((dst = icalcomponent_get_first_property(old, kind))) icalcomponent_remove_property(old, dst);
    // Add every property of this kind from new (handles repeats)
    for (guint i = 0; i < props->len; i++) {
      icalproperty *src = g_ptr_array_index(props, i);
      if (icalproperty_isa(src) == kind) {
        icalproperty *copy = icalproperty_clone(src);
        if (copy) icalcomponent_add_property(old, copy);
      }
    }
  }
  g_hash_table_destroy(by_kind);
  return true;
}

// ---------- IDLE CALLBACKS ---------- //

static gboolean remote_event_idle_cb(gpointer user_data) {
  g_assert_nonnull(user_data);

  const _CBData *data = user_data;
  switch (data->op) {

  case CAL_ADDED: {
    if (pushed_objs_is_stored(data->uid)) break;
    if (find_local_task_list(data->uid) != NULL) break;
    g_message("Sync: Remote list added: %s", data->uid);
    const char *name = caldav_calendar_get_name(data->calendar);
    const char *color = caldav_calendar_get_color(data->calendar);
    g_autoptr(ErrandsTaskListItem) item = errands_task_list_item_create(data->uid, name, color, false, true);
    g_list_store_append(task_lists_model, item);
    errands_task_list_item_save(item);
    errands_sidebar_update_filter_rows();
    errands_window_add_toast(tmp_str_printf(_("Added remote task list: %s"), name), 5);
  } break;

  case CAL_MODIFIED: {
    if (pushed_objs_is_stored(data->uid)) break;
    ErrandsTaskListItem *list = find_local_task_list(data->uid);
    if (!list) {
      g_warning("Sync: Couldn't find local list: %s", data->uid);
      break;
    }
    // Local list was modified first
    if (!errands_data_get_synced(list->ical)) break;
    if (data->name && !g_str_equal(data->name, list->title)) {
      errands_task_list_item_set_title(list, data->name);
      errands_task_list_update(state.main_window->task_list);
    }
    if (data->color) {
      const char *color = gdk_rgba_to_str(&list->color);
      if (!g_str_equal(data->color, color)) errands_task_list_item_set_color_from_string(list, data->color);
    }
    g_message("Sync: Calendar properties changed on remote: %s", data->uid);
  } break;

  case CAL_REMOVED: {
    g_message("Sync: Calendar removed on remote: %s", data->uid);
    ErrandsTaskListItem *list = find_local_task_list(data->uid);
    if (!list) {
      g_warning("Sync: Couldn't find local list: %s", data->uid);
      break;
    }
    errands_task_list_item_delete(list);
  } break;

  case TASK_ADDED: {
    if (pushed_objs_is_stored(data->uid)) break;
    ErrandsTaskListItem *list = find_local_task_list(caldav_calendar_get_uid(data->calendar));
    if (find_local_task(list, errands_data_get_uid(data->ical))) break;
    icalcomponent_add_component(list->ical, data->ical);
    const char *parent = errands_data_get_parent(data->ical);
    errands_data_set_synced(data->ical, true);
    ErrandsTaskItem *task_item = NULL;
    if (!parent) {
      task_item = errands_task_item_new(data->ical, list, NULL);
      g_list_store_append(list->tasks, task_item);
    } else {
      ErrandsTaskItem *parent_task_item = find_task_item(G_LIST_MODEL(list->tasks), parent);
      if (!parent_task_item) break;
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
    if (pushed_objs_is_stored(data->uid)) break;
    ErrandsTaskListItem *list = find_local_task_list(caldav_calendar_get_uid(data->calendar));
    if (!list) break;
    ErrandsTaskItem *task = find_task_item(G_LIST_MODEL(list->tasks), data->uid);
    if (!task) break;
    icalcomponent *old_ical = errands_task_item_get_ical(task);
    if (!merge_tasks(old_ical, data->ical)) break;
    errands_task_item_set_ical(task, old_ical);
    errands_data_set_synced(old_ical, true);
    errands_task_list_item_save(list);
    g_message("Sync: Remote task modified: %s", data->uid);
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

static void obj_pushed_idle_cb(gpointer user_data) {
  PushObj *o = user_data;

  g_assert_nonnull(o->uid);

  switch (o->op) {
  case PUSH_OBJ_LIST_PUSH: {
    if (!o->pushed) g_warning("Sync: Push remote list failed: %s", o->list->uid);
    else g_message("Sync: Pushed remote list: %s", o->list->uid);
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
    if (!o->pushed) g_warning("Sync: Push remote task failed: %s", o->uid);
    else g_message("Sync: Pushed remote task: %s", o->uid);
    icalcomponent *ical = errands_task_item_get_ical(o->task);
    errands_data_set_synced(ical, o->pushed);
    errands_task_list_item_save(errands_task_item_get_list(o->task));
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
  data->uid = g_strdup(caldav_calendar_get_uid(c));
  g_idle_add_full(G_PRIORITY_DEFAULT, remote_event_idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void cal_modified_cb(CalDAVCalendar *c, const char *name, const char *color) {
  g_assert_nonnull(c);

  _CBData *data = g_new0(_CBData, 1);
  data->op = CAL_MODIFIED;
  data->calendar = c;
  data->uid = g_strdup(caldav_calendar_get_uid(c));
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

  icalcomponent *task_ical = i_cal_object_get_native(I_CAL_OBJECT(ical));
  if (icalcomponent_isa(task_ical) != ICAL_VTODO_COMPONENT) return;

  _CBData *data = g_new0(_CBData, 1);
  data->op = TASK_ADDED;
  data->calendar = c;
  data->uid = g_strdup(errands_data_get_uid(task_ical));
  data->ical = icalcomponent_clone(task_ical);
  g_idle_add_full(G_PRIORITY_DEFAULT, remote_event_idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void task_modified_cb(CalDAVCalendar *c, ICalComponent *ical) {
  g_assert_nonnull(c);
  g_assert_nonnull(ical);

  icalcomponent *task_ical = i_cal_object_get_native(I_CAL_OBJECT(ical));
  if (icalcomponent_isa(task_ical) != ICAL_VTODO_COMPONENT) return;

  _CBData *data = g_new0(_CBData, 1);
  data->op = TASK_MODIFIED;
  data->calendar = c;
  data->uid = g_strdup(errands_data_get_uid(task_ical));
  data->ical = icalcomponent_clone(task_ical);
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

// Runs on separate thread
static void sync_thread_cb(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  if (!ensure_client_connected()) {
    g_warning("Sync: Client is not connected");
    g_task_return_boolean(task, FALSE);
    return;
  }
  for_range(i, 0, objects_to_push->len) obj_push(g_ptr_array_index(objects_to_push, i));
  g_task_return_boolean(task, caldav_client_poll(client));
}

// Runs on main thread
static void sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  sync_in_progress = FALSE;
  errands_sidebar_toggle_sync_indicator(FALSE);
  g_hash_table_remove_all(pushed_objs);
  gboolean success = g_task_propagate_boolean(G_TASK(res), NULL);
  if (!success) {
    g_warning("Sync: Error");
    errands_sync_cleanup();
    return;
  }
  guint interval = errands_settings_get_sync_interval(state.settings);
  g_timeout_add_seconds_once(interval, (GSourceOnceFunc)errands_sync, NULL);
  g_message("Sync: Finished. Scheduled next sync in %d seconds.", interval);
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

void errands_sync_init(void) {
  if (!objects_to_push) objects_to_push = g_ptr_array_new_with_free_func((GDestroyNotify)obj_free);
  if (!pushed_objs) pushed_objs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

void errands_sync_cleanup(void) {
  g_message("Sync: Cleanup");
  if (pushed_objs) g_hash_table_remove_all(pushed_objs);
  if (objects_to_push) g_ptr_array_set_size(objects_to_push, 0);
  g_clear_pointer(&pushed_objs, g_hash_table_destroy);
  g_clear_pointer(&objects_to_push, g_ptr_array_unref);
  g_clear_pointer(&client, caldav_client_free);
}

void errands_sync(void) {
  if (!errands_settings_get_sync_enabled(state.settings)) return;
  errands_sync_init();
  if (sync_in_progress) {
    guint interval = errands_settings_get_sync_interval(state.settings);
    g_timeout_add_seconds_once(interval, (GSourceOnceFunc)errands_sync, NULL);
    g_message("Sync: Can't start sync. Already in progress. Scheduled next sync in %d seconds.", interval);
    return;
  }
  g_message("Sync: Start");
  sync_in_progress = true;
  errands_sidebar_toggle_sync_indicator(TRUE);
  run_in_thread(sync_thread_cb, NULL, sync_finished_cb, NULL);
}

void errands_sync_push_list(ErrandsTaskListItem *list) {
  g_assert_nonnull(list);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(list));
  g_assert(list->uid);

  if (!errands_settings_get_sync_enabled(state.settings)) return;
  errands_sync_init();

  gboolean deleted = errands_data_get_deleted(list->ical);
  PushObj *o = objects_to_push_has(list->uid, deleted ? PUSH_OBJ_LIST_DELETE : PUSH_OBJ_LIST_PUSH);
  if (o) return;

  g_message("Sync: Push list %s", list->uid);

  o = push_obj_new(deleted ? PUSH_OBJ_LIST_DELETE : PUSH_OBJ_LIST_PUSH, list->uid);
  o->list = g_object_ref(list);
  o->name = g_strdup(list->title);
  o->color = g_strdup(gdk_rgba_to_str(&list->color));
  g_ptr_array_add(objects_to_push, o);
}

void errands_sync_push_task(ErrandsTaskItem *task) {
  g_assert_nonnull(task);
  g_assert(ERRANDS_IS_TASK_ITEM(task));

  if (!errands_settings_get_sync_enabled(state.settings)) return;
  errands_sync_init();

  icalcomponent *ical = errands_task_item_get_ical(task);
  const char *uid = errands_task_item_get_uid(task);
  enum PushObjOP op = errands_data_get_deleted(ical) ? PUSH_OBJ_TASK_DELETE : PUSH_OBJ_TASK_PUSH;
  PushObj *o = objects_to_push_has(uid, op);
  if (o) {
    o->count++;
    return;
  }

  o = push_obj_new(op, uid);
  o->list = g_object_ref(errands_task_item_get_list(task));
  o->task = g_object_ref(task);
  o->ical = icalcomponent_clone(ical);
  g_ptr_array_add(objects_to_push, o);
}
