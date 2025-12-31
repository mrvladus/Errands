#include "sync.h"
#include "data.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"
#include "window.h"
#include <libical/ical.h>

#define CALDAV_IMPLEMENTATION
#define CALDAV_DEBUG
#include "vendor/caldav.h"
#include "vendor/toolbox.h"

#include <glib/gi18n.h>

static bool sync_initialized = false;
static bool sync_scheduled = false;
static ListData *list_data = NULL;
static TaskData *task_data = NULL;
static CalDAVClient *client = NULL;

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
  // Connect client
  if (!caldav_client_connect(client, true)) {
    caldav_client_free(client);
    LOG("Sync: Unable to connect to CalDAV server");
    return false;
  }
  LOG("Sync: Connected to CalDAV server");

  sync_initialized = true;

  LOG("Sync: Initialized");

  return true;
}

static void errands__sync_cb(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  if (!sync_initialized && !errands__sync_init()) g_task_return_boolean(task, false);

  // Get calendars
  bool res = caldav_client_pull_calendars(client);
  if (!res) {
    LOG("Sync: Unable to get calendars");
    caldav_client_free(client);
    g_task_return_boolean(task, FALSE);
    return;
  }
  LOG("Sync: Loaded %zu calendars", client->calendars->len);

  // Get events
  for (size_t i = 0; i < client->calendars->len; i++) {
    CalDAVCalendar *cal = caldav_list_at(client->calendars, i);
    CONTINUE_IF_NOT(cal->set & CALDAV_COMPONENT_SET_VTODO);
    bool pulled = caldav_calendar_pull_events(cal);
    if (!pulled) LOG("Sync: Failed to pull events for calendar '%s'", cal->url);
  }

  g_task_return_boolean(task, true);
}

static void errands__sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  gtk_widget_set_visible(state.main_window->sidebar->sync_indicator, false);
  if (!g_task_propagate_boolean(G_TASK(res), NULL)) return;

  bool reload = false;
  for_range(i, 0, client->calendars->len) {
    CalDAVCalendar *cal = caldav_list_at(client->calendars, i);
    CONTINUE_IF_NOT(cal->set & CALDAV_COMPONENT_SET_VTODO);
    LOG("Sync: Processing calendar '%s'", cal->name);
    // Check if list with this UID exists
    ListData *existing_list = NULL;
    for_range(l, 0, errands_data_lists->len) {
      ListData *list = g_ptr_array_index(errands_data_lists, l);
      if (STR_EQUAL(list->uid, cal->uuid)) {
        existing_list = list;
        break;
      }
    }
    // Merge calendar data into existing list or create a new one
    if (existing_list) {
      icalcomponent_merge_component(existing_list->ical, cal->ical);
    } else {
      LOG("Sync: Create new list '%s'", cal->uuid);
      ListData *data = errands_list_data_load_from_ical(cal->ical, cal->uuid, cal->name, cal->color);
      g_ptr_array_add(errands_data_lists, data);
      reload = true;
    }
  }

  if (reload) {
    errands_sidebar_load_lists();
    errands_task_list_reload(state.main_window->task_list, false);
  }
}

void errands_sync_init(void) {
  gtk_widget_set_visible(state.main_window->sidebar->sync_indicator, true);
  caldav_init();
  RUN_THREAD_FUNC(errands__sync_cb, errands__sync_finished_cb);
  // g_timeout_add_seconds(10, G_SOURCE_FUNC(sync), NULL);
}

void errands_sync_cleanup(void) {
  caldav_client_free(client);
  caldav_cleanup();
}

void errands_sync_schedule(void) { sync_scheduled = true; }

void errands_sync_schedule_list(ListData *data) {
  sync_scheduled = true;
  list_data = data;
}

void errands_sync_schedule_task(TaskData *data) {
  sync_scheduled = true;
  task_data = data;
}
