// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "sync.h"
#include "caldav.h"
#include "data.h"
#include "glib-object.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "task-item.h"
#include "task-list-item.h"
#include "task-list.h"
#include "utils.h"

#include <gio/gio.h>
#include <glib.h>
#include <libical-glib/libical-glib.h>

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
  ICalComponent *task;
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

// ---------- GLOBAL ---------- //

static CalDAVClient *client = NULL;
static GPtrArray *deleting_calendars = NULL;

// ---------- HELPERS ---------- //

static inline ErrandsTaskListItem *find_local_task_list(const char *uid) {
  GListModel *model = G_LIST_MODEL(task_lists_model);
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskListItem) item = g_list_model_get_item(model, i);
    if (g_str_equal(item->uid, uid)) return item;
  }
  return NULL;
}

static inline ICalComponent *find_local_task(ErrandsTaskListItem *list, const char *uid) {
  g_autoptr(GPtrArray) tasks = vcalendar_to_array(list->ical);
  for_range(i, 0, tasks->len) {
    ICalComponent *task = g_ptr_array_index(tasks, i);
    const char *task_uid = errands_data_get_uid(task);
    if (g_str_equal(uid, task_uid)) return task;
  }
  return NULL;
}

static inline ErrandsTaskItem *find_task_item(GListModel *model, const char *uid) {
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    if (g_str_equal(uid, errands_task_item_get_uid(item))) return item;
    ErrandsTaskItem *child = find_task_item(errands_task_item_get_children_model(item), uid);
    if (child) return child;
  }
  return NULL;
}

static inline bool merge_tasks(ICalComponent *dest, ICalComponent *source) {
  g_assert_nonnull(dest);
  g_assert_nonnull(source);

  bool changed = false;
  g_autoptr(GPtrArray) props = g_ptr_array_new_with_free_func(g_object_unref);
  for (ICalProperty *prop = i_cal_component_get_first_property(source, I_CAL_ANY_PROPERTY); prop;
       prop = i_cal_component_get_next_property(source, I_CAL_ANY_PROPERTY)) {
    g_ptr_array_add(props, prop);
  }
  for (guint i = 0; i < props->len; i++) {
    ICalProperty *src = g_ptr_array_index(props, i);
    ICalPropertyKind kind = i_cal_property_isa(src);
    g_autoptr(ICalProperty) dst = i_cal_component_get_first_property(dest, kind);
    if (dst) {
      g_autofree gchar *src_str = i_cal_property_as_ical_string(src);
      g_autofree gchar *dst_str = i_cal_property_as_ical_string(dst);
      if (g_strcmp0(src_str, dst_str) == 0) continue;
      i_cal_component_remove_property(dest, dst);
    }
    i_cal_component_add_property(dest, i_cal_property_clone(src));
    changed = true;
  }
  return changed;
}

// ---------- IDLE CALLBACKS ---------- //

static gboolean idle_cb(gpointer user_data) {
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
    errands_sidebar_update_filter_rows();
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
      char color[8];
      gdk_rgba_to_hex_string(&list->color, color);
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
    errands_task_list_item_delete(list);
  } break;

  case TASK_ADDED: {
    ErrandsTaskListItem *list = find_local_task_list(caldav_calendar_get_uid(data->calendar));
    if (find_local_task(list, i_cal_component_get_uid(data->task))) {
      i_cal_component_free(data->task);
      return G_SOURCE_REMOVE;
    }
    i_cal_component_add_component(list->ical, data->task);
    const char *parent = errands_data_get_parent(data->task);
    if (!parent) {
      ErrandsTaskItem *task_item = errands_task_item_new(data->task, list, NULL);
      g_list_store_append(list->tasks, task_item);
    } else {
      ErrandsTaskItem *parent_task_item = find_task_item(G_LIST_MODEL(list->tasks), parent);
      if (!parent_task_item) return G_SOURCE_REMOVE;
      GListModel *child_model = errands_task_item_get_children_model(parent_task_item);
      ErrandsTaskItem *task_item = errands_task_item_new(data->task, list, parent_task_item);
      g_list_store_append(G_LIST_STORE(child_model), task_item);
    }
    errands_task_list_item_update_count(list);
    errands_sidebar_update_filter_rows();
    errands_task_list_update(state.main_window->task_list);

    g_message("Sync: Task added to list '%s': '%s'", list->uid, errands_data_get_uid(data->task));
  } break;

  case TASK_MODIFIED: {
    const char *list_uid = caldav_calendar_get_uid(data->calendar);
    ErrandsTaskListItem *list = find_local_task_list(list_uid);
    if (!list) {
      g_warning("Sync: Failed to find local list: %s", list_uid);
      return G_SOURCE_REMOVE;
    }
    const char *uid = errands_data_get_uid(data->task);
    ErrandsTaskItem *task = find_task_item(G_LIST_MODEL(list->tasks), uid);
    if (!task) {
      g_warning("Sync: Failed to find local task: %s", uid);
      return G_SOURCE_REMOVE;
    }
    ICalComponent *ical = errands_task_item_get_ical(task);
    bool changed = merge_tasks(ical, data->task);
    i_cal_component_free(data->task);
    errands_task_item_set_ical(task, ical);
    g_message("Sync: Task modified: %s", errands_task_item_get_uid(task));
  } break;

  case TASK_REMOVED: {
    g_message("Sync: Task %s removed from remote list %s", data->uid, caldav_calendar_get_uid(data->calendar));
    g_return_val_if_reached(G_SOURCE_REMOVE);
  } break;
  }

  return G_SOURCE_REMOVE;
}

// ---------- CALLBACKS ---------- //

static void cal_added_cb(CalDAVCalendar *c) {
  _CBData *data = g_new0(_CBData, 1);
  data->op = CAL_ADDED;
  data->calendar = c;
  g_idle_add_full(G_PRIORITY_DEFAULT, idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void cal_modified_cb(CalDAVCalendar *c, const char *name, const char *color) {
  _CBData *data = g_new0(_CBData, 1);
  data->op = CAL_MODIFIED;
  data->calendar = c;
  data->name = name ? g_strdup(name) : NULL;
  data->color = color ? g_strdup(color) : NULL;
  g_idle_add_full(G_PRIORITY_DEFAULT, idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void cal_removed_cb(const char *uid) {
  _CBData *data = g_new0(_CBData, 1);
  data->op = CAL_REMOVED;
  data->uid = uid ? g_strdup(uid) : NULL;
  g_idle_add_full(G_PRIORITY_DEFAULT, idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void task_added_cb(CalDAVCalendar *c, ICalComponent *task) {
  _CBData *data = g_new0(_CBData, 1);
  data->op = TASK_ADDED;
  data->calendar = c;
  data->task = i_cal_component_clone(task);
  g_idle_add_full(G_PRIORITY_DEFAULT, idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void task_modified_cb(CalDAVCalendar *c, ICalComponent *task) {
  _CBData *data = g_new0(_CBData, 1);
  data->op = TASK_MODIFIED;
  data->calendar = c;
  data->task = i_cal_component_clone(task);
  g_idle_add_full(G_PRIORITY_DEFAULT, idle_cb, data, (GDestroyNotify)cb_data_free);
}

static void task_removed_cb(CalDAVCalendar *c, const char *uid) {
  _CBData *data = g_new0(_CBData, 1);
  data->op = TASK_REMOVED;
  data->calendar = c;
  data->uid = uid ? g_strdup(uid) : NULL;
  g_idle_add_full(G_PRIORITY_DEFAULT, idle_cb, data, (GDestroyNotify)cb_data_free);
}

// ---------- WORKER THREADS ---------- //

static void sync_thread_cb(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
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
  caldav_client_poll(client);
}

static void sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) { g_message("Sync: Finished"); }

static void cal_delete_thread_cb(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  CalDAVCalendar *c = task_data;
  g_autofree gchar *uid = g_strdup(caldav_calendar_get_uid(c));
  g_message("Sync: Deleting list on remote: %s", uid);
  g_task_return_boolean(task, caldav_calendar_remove(c));
}

static void cal_delete_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  g_autoptr(GError) error = NULL;
  gboolean success = g_task_propagate_boolean(G_TASK(res), &error);
  ErrandsTaskListItem *list = user_data;
  if (error) g_warning("Sync: Deleting remote list failed: %s", error->message);
  else g_message("Sync: Deleting remote list success: %s", success ? "yes" : "no");
  errands_data_set_synced(list->ical, success);
  errands_task_list_item_save(list);
  g_ptr_array_remove(deleting_calendars, list);
}

// ---------- PUBLIC ---------- //

void errands_sync_cleanup(void) {
  g_message("Sync: Cleanup");
  if (client) caldav_client_free(client);
  g_clear_object(&deleting_calendars);
}

gboolean errands_sync(void) {
  if (!errands_settings_get_sync_enabled(state.settings)) return G_SOURCE_REMOVE;
  if (!deleting_calendars) deleting_calendars = g_ptr_array_new_full(8, g_object_unref);
  g_message("Sync: Start");
  run_in_thread(sync_thread_cb, NULL, sync_finished_cb, NULL);
  return G_SOURCE_CONTINUE;
}

void errands_sync_delete_list(ErrandsTaskListItem *list) {
  ErrandsTaskListItem *l = g_object_ref(list);
  for_range(i, 0, deleting_calendars->len) if (g_ptr_array_index(deleting_calendars, i) == l) return;
  g_ptr_array_add(deleting_calendars, l);
  CalDAVCalendar *c = caldav_client_find_calendar(client, l->uid);
  if (c) run_in_thread(cal_delete_thread_cb, c, cal_delete_finished_cb, l);
}

void errands_sync_create_list(ErrandsTaskListItem *list) {}

void errands_sync_update_list(ErrandsTaskListItem *list) {}

void errands_sync_delete_task(ErrandsTaskItem *task) {}

void errands_sync_create_task(ErrandsTaskItem *task) {}

void errands_sync_update_task(ErrandsTaskItem *task) {}
