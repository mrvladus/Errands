#include "sync.h"
#include "settings.h"
#include "state.h"

// ---------- GLOBAL ---------- //

// ---------- PRIVATE ---------- //

static bool task_is_changed(ErrandsTaskItem *task) { return true; }

static void sync_cb(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  //   if (!client) {
  //     const char *url = errands_settings_get_sync_url(state.settings);
  //     const char *username = errands_settings_get_sync_username(state.settings);
  //     const char *password = errands_settings_get_sync_password(state.settings);
  //     client = caldav_client_new(url, username, password);
  //   }
  //
  //   caldav_client_pull_calendars(client);
  //   for (size_t i = 0; i < client->calendars->count; i++) {
  //     CalDAVCalendar *calendar = client->calendars->items[i];
  //     if (calendar->component_set != CALDAV_COMPONENT_SET_VTODO) continue;
  //     g_message("Sync: found remote calendar: %s\n", calendar->display_name);
  //     caldav_calendar_pull_events(calendar);
  //     for (size_t j = 0; j < calendar->events->count; j++) {
  //       CalDAVEvent *event = calendar->events->items[j];
  //       g_message("Sync: found remote event: %s\n", event->href);
  //     }
  //   }
}

static void sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {}

// ---------- PUBLIC ---------- //

void errands_sync_cleanup(void) {}

void errands_sync(void) {
  if (!errands_settings_get_sync_enabled(state.settings)) return;

  GTask *task = g_task_new(NULL, NULL, sync_finished_cb, NULL);
  g_task_run_in_thread(task, sync_cb);
}

void errands_sync_delete_list(ErrandsTaskListItem *list) {}

void errands_sync_create_list(ErrandsTaskListItem *list) {}

void errands_sync_update_list(ErrandsTaskListItem *list) {}

void errands_sync_delete_task(ErrandsTaskItem *task) {}

void errands_sync_create_task(ErrandsTaskItem *task) {}

void errands_sync_update_task(ErrandsTaskItem *task) {}
