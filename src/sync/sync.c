#include "sync.h"
#include "../settings.h"
#include "../utils.h"
#include "caldav/caldav.h"

#include <glib.h>

#include <libical/ical.h>

CalDAVClient *client;

// void sync_list(ListData *list) {
//   if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b) {
//     LOG("Sync: Sync is disabled");
//     return;
//   }
// }
// void sync_task(TaskData *task) {
//   if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b) {
//     LOG("Sync: Sync is disabled");
//     return;
//   }
// }

// This is the asynchronous work function that runs in a separate thread.
static void initial_sync(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  LOG("Sync: Initialize");
  // Check if sync is disabled.
  if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b) {
    LOG("Sync: Sync is disabled");
    g_task_return_boolean(task, FALSE);
    return;
  }
  // Create client
  client = caldav_client_new("http://localhost:8080", "vlad", "1710");
  if (!client) {
    LOG("Sync: Unable to connect to CalDAV server");
    g_task_return_boolean(task, FALSE);
    return;
  }
  // Get calendars
  bool res = caldav_client_pull_calendars(client, CALDAV_COMPONENT_SET_VTODO);
  if (!res || !client->calendars) {
    LOG("Sync: Unable to get calendars");
    caldav_client_free(client);
    g_task_return_boolean(task, FALSE);
    return;
  }
  LOG("Sync: Loaded %zu calendars", client->calendars->len);
  // Get events
  for (size_t i = 0; i < client->calendars->len; i++) {
    CalDAVCalendar *calendar = caldav_list_at(client->calendars, i);
    caldav_calendar_print(calendar);
    bool res = caldav_calendar_pull_events(calendar);
    // if (res && calendar->events) {
    //   for (size_t idx = 0; idx < calendar->events->len; idx++) {
    //     CalDAVEvent *event = caldav_list_at(calendar->events, idx);
    //     caldav_event_print(event);
    //   }
    // }
  }
  // Return TRUE if sync was successful
  g_task_return_boolean(task, TRUE);
}

// Callback that runs on the main (UI) thread when the task is complete.
static void initial_sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  gboolean success = g_task_propagate_boolean(G_TASK(res), NULL);
  if (success) {
    LOG("Sync: Completed successfully");
    bool changed = false;
    for (size_t i = 0; i < client->calendars->len; i++) {
      CalDAVCalendar *calendar = caldav_list_at(client->calendars, i);
      for (size_t idx = 0; idx < calendar->events->len; idx++) {
        // Get event
        CalDAVEvent *event = caldav_list_at(calendar->events, idx);
        // Get UID and DTSTAMP
        const char *uid = icalcomponent_get_uid(event->ical);
        icaltimetype dtstamp = icalcomponent_get_dtstamp(event->ical);
        if (g_hash_table_contains(tdata, uid)) {
          // TaskData *td = task_data_new_from_ical(event->ical);
        }
        // bool found = false;
        // GPtrArray* tasks = g_hash_table_get_values_as_ptr_array(tdata);
        // for (size_t j = 0; j < tasks->len; ++j) {
        //   const char *td_uid = icalcomponent_get_uid((TaskData *)tasks->pdata[j]);
        //   if (!strcmp(uid, td_uid)) {
        //     found = true;
        //     icaltimetype td_dtstamp = icalcomponent_get_dtstamp((TaskData *)tasks->pdata[j]);
        //     // If remote task changed after local - replace it
        //     if (icaltime_compare(dtstamp, td_dtstamp) == 1) {}
        //   }
        // }
        // if (!found) {
        //     const char *td_parent = icalcomponent_get_uid(tdata->pdata);
        //   errands_task_list_add(event->ical);
        // }
      }
    }
  } else {
    LOG("Sync: Failed or canceled");
  }
}

void sync_init(void) { RUN_THREAD_FUNC(initial_sync, initial_sync_finished_cb); }
