#include "sync.h"
#include "data.h"
#include "glib.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "utils.h"
#include "window.h"
#include <libical/ical.h>
#include <stddef.h>

// #define CALDAV_DEBUG
#define CALDAV_IMPLEMENTATION
#include "vendor/caldav.h"
#include "vendor/toolbox.h"

#include <glib/gi18n.h>

static bool sync_initialized = false;
static bool sync_scheduled = false;
static bool sync_in_progress = false;

static CalDAVClient *client = NULL;

static GPtrArray *lists_to_delete_on_server_copy = NULL, *lists_deleted_on_server = NULL;
static GPtrArray *lists_to_create_on_server = NULL, *lists_to_create_on_server_copy = NULL,
                 *lists_created_on_server = NULL;
static GPtrArray *lists_to_update_on_server = NULL, *lists_to_update_on_server_copy = NULL,
                 *lists_updated_on_server = NULL;

GPtrArray *lists_to_delete_on_server = NULL;

// --- UTILS --- //

static CalDAVCalendar *find_calendar_by_uid(const char *uid) {
  for_range(i, 0, client->calendars->count) {
    CalDAVCalendar *c = client->calendars->items[i];
    CONTINUE_IF_NOT(c->component_set & CALDAV_COMPONENT_SET_VTODO);
    if (STR_EQUAL(c->uid, uid)) return c;
  }
  return NULL;
}

// TODO: event shoud have uid
static CalDAVEvent *find_event_by_uid(CalDAVCalendar *c, const char *uid) {
  for_range(i, 0, c->events->count) {
    CalDAVEvent *e = c->events->items[i];
    autoptr(icalcomponent) outer = icalcomponent_new_from_string(e->ical);
    CONTINUE_IF_NOT(outer);
    icalcomponent *inner = icalcomponent_get_first_component(outer, ICAL_VTODO_COMPONENT);
    CONTINUE_IF_NOT(inner);
    const char *uid_prop = icalcomponent_get_uid(inner);
    if (STR_EQUAL(uid, uid_prop)) return e;
  }

  return NULL;
}

static icalcomponent *caldav_calendar_to_icalcomponent(CalDAVCalendar *c) {
  icalcomponent *ical = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
  icalcomponent_add_property(ical, icalproperty_new_version("2.0"));
  icalcomponent_add_property(ical, icalproperty_new_prodid("~//Errands"));
  errands_data_set_synced(ical, true);
  errands_data_set_list_name(ical, c->display_name);
  errands_data_set_list_description(ical, c->description);
  errands_data_set_color(ical, c->color, true);
  for_range(i, 0, c->events->count) {
    CalDAVEvent *e = c->events->items[i];
    icalcomponent *event = icalcomponent_new_from_string(e->ical);
    if (event) icalcomponent_merge_component(ical, event);
  }

  return ical;
}

// --- SYNC --- //

static bool errands__sync_init(void) {
  // Check if sync is disabled.
  if (!errands_settings_get(SETTING_SYNC).b) {
    LOG("Sync: Sync is disabled. Do nothing.");
    return false;
  }
  LOG("Sync: Initialize");
  // Get credentials
  const char *url = errands_settings_get(SETTING_SYNC_URL).s;
  const char *username = errands_settings_get(SETTING_SYNC_USERNAME).s;
  const char *password = errands_settings_get_password();
  if (!url || !username || !password) {
    LOG("Sync: Missing required CalDAV credentials");
    g_idle_add_once((GSourceOnceFunc)errands_window_add_toast, _("Missing sync credentials"));
    return false;
  }
  // Create client
  client = caldav_client_new(url, username, password);
  if (!client) {
    LOG("Sync: Failed to create CalDAVClient");
    return false;
  }
  LOG("Sync: Connected to CalDAV server");
  sync_initialized = true;
  LOG("Sync: Initialized");
  return true;
}

static void errands__sync_delete_lists_on_server(void) {
  for_range(i, 0, lists_to_delete_on_server_copy->len) {
    ListData *list = g_ptr_array_index(lists_to_delete_on_server_copy, i);
    CalDAVCalendar *c = find_calendar_by_uid(list->uid);
    CONTINUE_IF_NOT(c);
    LOG("Sync: Deleting calendar on server %s", list->uid);
    errands_data_set_synced(list->ical, caldav_calendar_delete(c));
    errands_list_data_save(list);
    g_ptr_array_add(lists_deleted_on_server, list);
  }
}

// static void errands__sync_create_lists_on_server(void) {
//   for_range(i, 0, lists_to_create_on_server_copy->len) {
//     ListData *list = g_ptr_array_index(lists_to_create_on_server_copy, i);
//     CalDAVCalendar *c = find_calendar_by_uid(list->uid);
//     LOG("Sync: Creating calendar on server: %s", list->uid);
//     bool created = caldav_client_create_calendar(client, list->uid, errands_data_get_list_name(list->ical), NULL,
//                                                  errands_data_get_color(list->ical, true),
//                                                  CALDAV_COMPONENT_SET_VTODO);
//     if (created) {
//       // TODO: we resetting changed state for calendar. Maybe separate func in caldav.h like
//       // caldav_calendar_reset_state()
//       caldav_client_pull_calendars(client);
//       g_autoptr(GPtrArray) tasks = g_ptr_array_sized_new(list->children->len);
//       errands_data_get_flat_list(tasks);
//       g_ptr_array_extend(lists_created_on_server, tasks, NULL, NULL);
//       g_ptr_array_add(lists_deleted_on_server, list);
//     }
//   }
// }

// static void errands__sync_update_lists_on_server(void) {
//   for_range(i, 0, lists_to_update_on_server_copy->len) {
//     ListData *list = g_ptr_array_index(lists_to_update_on_server_copy, i);
//     CalDAVCalendar *c = find_calendar_by_uid(list->uid);
//     CONTINUE_IF(!c || errands_data_get_synced(list->ical));
//     bool needs_update = false;
//     const char *name = errands_data_get_list_name(list->ical);
//     const char *color = errands_data_get_color(list->ical, true);
//     if (!STR_EQUAL(name, c->display_name)) needs_update = true;
//     if (!STR_EQUAL(color, c->color)) needs_update = true;
//     if (needs_update) {
//       LOG("Sync: Updating list properties on server: %s", list->uid);
//       caldav_calendar_update(c, name, NULL, color);
//     }
//   }
// }

static void errands__sync_delete_tasks_on_server(void) {}
static void errands__sync_create_tasks_on_server(void) {}
static void errands__sync_update_tasks_on_server(void) {}

// static void errands__sync_cb_push_tasks(void) {
//   for_range(i, 0, tasks_to_push_copy->len) {
//     TaskData *data = g_ptr_array_index(tasks_to_push_copy, i);
//     if (!data->list) continue;
//     const char *uid = errands_data_get_uid(data->ical);
//     CalDAVCalendar *c = find_calendar_by_uid(data->list->uid);
//     g_assert(c);
//     // Delete task
//     if (errands_data_get_deleted(data->ical)) {
//       CalDAVEvent *e = find_event_by_uid(c, uid);
//       if (e) {
//         LOG("Sync: Deleting task on server: %s", uid);
//         caldav_event_delete(e);
//       }
//       continue;
//     }
//     CalDAVEvent *existing_event = find_event_by_uid(c, uid);
//     autoptr(icalcomponent) ical = icalcomponent_new_vcalendar();
//     icalcomponent_add_component(ical, icalcomponent_new_clone(data->ical));
//     // Create task
//     if (!existing_event) {
//       LOG("Sync: Creating event on server: %s", uid);
//       caldav_calendar_create_event(c, uid, icalcomponent_as_ical_string(ical));
//     }
//     // Update task on server
//     else {
//       autoptr(icalcomponent) vcalendar = icalcomponent_new_from_string(existing_event->ical);
//       icalcomponent *vtodo = icalcomponent_get_first_component(vcalendar, ICAL_VTODO_COMPONENT);
//       CONTINUE_IF_NOT(vtodo);
//       icaltimetype changed_on_server = errands_data_get_changed(vtodo);
//       icaltimetype changed_on_client = errands_data_get_changed(data->ical);
//       if (icaltime_compare(changed_on_client, changed_on_server) == 1) {
//         LOG("Sync: Updating event on server: %s", uid);
//         caldav_event_update(existing_event, icalcomponent_as_ical_string(ical));
//       }
//     }
//     g_ptr_array_add(tasks_pushed, data);
//   }
// }

// Runs in separate thread to not block the UI
static void errands__sync_cb(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  if (!sync_initialized) {
    if (!errands__sync_init()) {
      g_task_return_boolean(task, false);
      return;
    }
  }
  // Get calendars
  caldav_client_pull_calendars(client);
  // Get events
  for (size_t i = 0; i < client->calendars->count; i++) {
    CalDAVCalendar *cal = client->calendars->items[i];
    CONTINUE_IF_NOT(cal->component_set & CALDAV_COMPONENT_SET_VTODO);
    if (cal->events_changed) caldav_calendar_pull_events(cal);
  }
  errands__sync_delete_lists_on_server();
  // errands__sync_cb_push_tasks();

  g_task_return_boolean(task, true);
}

// Runs in main thread on sync finish
static void errands__sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  errands_sidebar_toggle_sync_indicator(false);
  if (!g_task_propagate_boolean(G_TASK(res), NULL)) return;
  // Cleanup synced lists
  guint idx = 0;
  // for_range(i, 0, lists_pushed->len) {
  //   ListData *data = g_ptr_array_index(lists_pushed, i);
  //   errands_list_data_save(data);
  //   if (g_ptr_array_find(lists_to_push, data, &idx)) g_ptr_array_remove_index_fast(lists_to_push, idx);
  // }
  // // Cleanup synced tasks
  // for_range(i, 0, tasks_pushed->len) {
  //   TaskData *data = g_ptr_array_index(tasks_pushed, i);
  //   if (g_ptr_array_find(tasks_to_push, data, &idx)) g_ptr_array_remove_index_fast(tasks_to_push, idx);
  // }
  bool reload = false;
  for_range(i, 0, client->calendars->count) {
    CalDAVCalendar *cal = client->calendars->items[i];
    CONTINUE_IF_NOT(cal->component_set & CALDAV_COMPONENT_SET_VTODO);
    ListData *existing_list = errands_data_find_list_data_by_uid(cal->uid);
    if (!existing_list) {
      LOG("Sync: Create new list: %s", cal->uid);
      icalcomponent *ical = caldav_calendar_to_icalcomponent(cal);
      ListData *new_list = errands_list_data_load_from_ical(ical, cal->uid, cal->display_name, cal->color);
      g_ptr_array_add(errands_data_lists, new_list);
      errands_list_data_save(new_list);
    } else {
      // Update local properties
      bool props_changed = false;
      const char *curr_name = errands_data_get_list_name(existing_list->ical);
      const char *curr_color = errands_data_get_color(existing_list->ical, true);
      if (!STR_EQUAL(curr_name, cal->display_name)) {
        errands_data_set_list_name(existing_list->ical, cal->display_name);
        props_changed = true;
      }
      if (!STR_EQUAL(curr_color, cal->color)) {
        errands_data_set_color(existing_list->ical, cal->color, true);
        props_changed = true;
      }
      if (props_changed) {
        LOG("Sync: Update list properties: %s", cal->uid);
        errands_list_data_save(existing_list);
      }
      // Create local tasks
      // Update local tasks
    }
    reload = true;
  }

  if (reload) {
    errands_sidebar_load_lists();
    errands_task_list_reload(state.main_window->task_list, false);
  }
  sync_in_progress = false;
  sync_scheduled = false;
  LOG("Sync: Finished");
}

static bool errands__sync() {
  if (!sync_scheduled || sync_in_progress) return true;
  LOG("Sync: Started");
  sync_in_progress = true;
  // Reset and copy data
  g_ptr_array_set_size(lists_deleted_on_server, 0);
  g_ptr_array_set_size(lists_to_delete_on_server_copy, 0);
  g_ptr_array_extend(lists_to_delete_on_server_copy, lists_to_delete_on_server, NULL, NULL);

  errands_sidebar_toggle_sync_indicator(true);
  RUN_THREAD_FUNC(errands__sync_cb, errands__sync_finished_cb);

  return true;
}

void errands_sync_init(void) {
  lists_to_delete_on_server = g_ptr_array_sized_new(8);
  lists_to_delete_on_server_copy = g_ptr_array_sized_new(8);
  lists_deleted_on_server = g_ptr_array_sized_new(8);

  lists_to_create_on_server = g_ptr_array_sized_new(8);
  lists_to_create_on_server_copy = g_ptr_array_sized_new(8);
  lists_created_on_server = g_ptr_array_sized_new(8);

  lists_to_update_on_server = g_ptr_array_sized_new(8);
  lists_to_update_on_server_copy = g_ptr_array_sized_new(8);
  lists_updated_on_server = g_ptr_array_sized_new(8);

  RUN_THREAD_FUNC(errands__sync_cb, errands__sync_finished_cb);
  int interval = errands_settings_get(SETTING_SYNC_INTERVAL).i;
  g_timeout_add_seconds(interval >= 10 ? interval : 10, G_SOURCE_FUNC(errands__sync), NULL);
}

void errands_sync_cleanup(void) {
  if (client) caldav_client_free(client);
}

void errands_sync(void) {
  sync_scheduled = true;
  errands__sync();
}

// void errands_sync_schedule_list(ListData *data) {
//   LOG("Sync: Schedule List: %s", data->uid);
//   sync_scheduled = true;
//   g_ptr_array_add(lists_to_push, data);
// }

// void errands_sync_schedule_task(TaskData *data) {
//   LOG("Sync: Schedule Task: %s", errands_data_get_uid(data->ical));
//   g_ptr_array_add(tasks_to_push, data);
//   sync_scheduled = true;
// }
