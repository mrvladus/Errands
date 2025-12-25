#include "sync.h"
#include "data.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

#define CALDAV_IMPLEMENTATION
// #define CALDAV_DEBUG
#include "vendor/caldav.h"
#include "vendor/toolbox.h"

// static bool sync_initialized = false;
static bool sync_scheduled = false;
static ListData *list_data = NULL;
static TaskData *task_data = NULL;
static CalDAVClient *client = NULL;

// void sync_list(ErrandsData *list) {
//   if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b) {
//     LOG("Sync: Sync is disabled");
//     return;
//   }
// }
// void sync_task(ErrandsData *task) {
//   if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b) {
//     LOG("Sync: Sync is disabled");
//     return;
//   }
// }

static void initial_sync(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  LOG("Sync: Initialize");
  // Check if sync is disabled.
  if (!errands_settings_get(SETTING_SYNC).b) {
    g_task_return_boolean(task, FALSE);
    LOG("Sync: Sync is disabled");
    return;
  }
  const char *url = errands_settings_get(SETTING_SYNC_URL).s;
  const char *username = errands_settings_get(SETTING_SYNC_USERNAME).s;
  const char *password = errands_settings_get_password();
  if (!url || !username || !password) {
    LOG("Sync: Missing required CalDAV credentials");
    g_task_return_boolean(task, FALSE);
    return;
  }
  client = caldav_client_new(url, username, password);
  if (!client) {
    LOG("Sync: Unable to connect to CalDAV server");
    g_task_return_boolean(task, FALSE);
    return;
  }
  bool res = caldav_client_connect(client, true);
  if (!res) {
    LOG("Sync: Unable to connect to CalDAV server");
    caldav_client_free(client);
    g_task_return_boolean(task, FALSE);
    return;
  }
  // Get calendars
  res = caldav_client_pull_calendars(client, CALDAV_COMPONENT_SET_VTODO);
  if (!res || !client->calendars) {
    LOG("Sync: Unable to get calendars");
    caldav_client_free(client);
    g_task_return_boolean(task, FALSE);
    return;
  }
  // Get events
  for (size_t i = 0; i < client->calendars->len; i++) caldav_calendar_pull_events(caldav_list_at(client->calendars, i));
  g_task_return_boolean(task, TRUE);
}

// Callback that runs on the main (UI) thread when the task is complete.
static void initial_sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  gboolean success = g_task_propagate_boolean(G_TASK(res), NULL);
  if (!success) return;
  for (size_t i = 0; i < client->calendars->len; i++) {
    CalDAVCalendar *calendar = caldav_list_at(client->calendars, i);
    ListData *list = errands_list_data_create(calendar->uuid, calendar->name, calendar->color, false, false);
    g_ptr_array_add(errands_data_lists, list);
    ErrandsSidebarTaskListRow *row = errands_sidebar_add_task_list(state.main_window->sidebar, list);
    for_range(j, 0, calendar->events->len) {
      CalDAVEvent *event = caldav_list_at(calendar->events, j);
      icalcomponent *ical = caldav_event_get_ical_event(event);
      errands_task_data_new(ical, NULL, list);
    }
    errands_sidebar_task_list_row_update(row);
  }
  errands_data_sort();
  errands_task_list_reload(state.main_window->task_list, false);
}

void errands_sync_init(void) {
  RUN_THREAD_FUNC(initial_sync, initial_sync_finished_cb);
  // g_timeout_add_seconds(10, G_SOURCE_FUNC(sync), NULL);
}

void errands_sync_cleanup() { caldav_client_free(client); }

void errands_sync_schedule() { sync_scheduled = true; }

void errands_sync_schedule_list(ListData *data) {
  sync_scheduled = true;
  list_data = data;
}

void errands_sync_schedule_task(TaskData *data) {
  sync_scheduled = true;
  task_data = data;
}
