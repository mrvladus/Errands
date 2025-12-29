#include "sync.h"
#include "data.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "utils.h"

#define CALDAV_IMPLEMENTATION
#include "vendor/caldav.h"
#include "vendor/toolbox.h"

// static bool sync_initialized = false;
static bool sync_scheduled = false;
static ListData *list_data = NULL;
static TaskData *task_data = NULL;
static CalDAVClient *client = NULL;

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
  LOG("Sync: Connected to CalDAV server");
  // Get calendars
  res = caldav_client_pull_calendars(client, CALDAV_COMPONENT_SET_VTODO);
  if (!res || !client->calendars) {
    LOG("Sync: Unable to get calendars");
    caldav_client_free(client);
    g_task_return_boolean(task, FALSE);
    return;
  }
  LOG("Sync: Found %zu calendars on server", client->calendars->len);
  // Get events
  for (size_t i = 0; i < client->calendars->len; i++) {
    CalDAVCalendar *cal = caldav_list_at(client->calendars, i);
    bool pulled = caldav_calendar_pull_events(cal);
    if (!pulled) LOG("Sync: Failed to pull events for calendar '%s'", cal->url);
  }
  g_task_return_boolean(task, TRUE);
}

// Callback that runs on the main (UI) thread when the task is complete.
static void initial_sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  if (!g_task_propagate_boolean(G_TASK(res), NULL)) return;

  for_range(i, 0, client->calendars->len) {
    CalDAVCalendar *cal = caldav_list_at(client->calendars, i);
    ListData *data = errands_list_data_load_from_ical(cal->ical, cal->uuid, cal->name, cal->color);
    g_ptr_array_add(errands_data_lists, data);
    errands_sidebar_add_task_list(state.main_window->sidebar, data);
  }

  LOG("Sync: Initial sync done");
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
