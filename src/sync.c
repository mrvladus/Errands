#include "sync.h"
#include "data.h"
#include "glib.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "utils.h"
#include "window.h"
#include <libical/ical.h>

#define CALDAV_DEBUG
#define CALDAV_IMPLEMENTATION
#include "vendor/caldav.h"
#include "vendor/toolbox.h"

#include <glib/gi18n.h>

static bool sync_initialized = false;
static bool sync_scheduled = false;
static bool sync_in_progress = false;
static ListData *list_data = NULL;
static CalDAVClient *client = NULL;
static GPtrArray *tasks_to_push = NULL;
static GPtrArray *tasks_pushed = NULL;

// --- UTILS --- //

// TODO: move to caldav.h
// TODO: calendar should have uid
static const char *get_calendar_uid_from_href(const char *href) {
  char *last_slash = strchr(href, '/');
  bool ends_with_slash = false;
  while (last_slash) {
    char *next_slash = strchr(last_slash + 1, '/');
    if (!next_slash) break;
    if (*(next_slash + 1) == '\0') {
      ends_with_slash = true;
      break;
    }
    last_slash = next_slash;
  }
  char *out = (char *)tmp_str_printf("%s", last_slash + 1);
  if (ends_with_slash) out[strlen(out) - 1] = '\0';
  return (const char *)out;
}

static CalDAVCalendar *find_calendar_by_uid(const char *uid) {
  for_range(i, 0, client->calendars->count) {
    CalDAVCalendar *c = client->calendars->items[i];
    // TODO: use c->uid
    if (strstr(c->href, uid)) return c;
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

// Runs in thread
static void errands__sync_cb(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  if (!sync_initialized) {
    if (!errands__sync_init()) {
      g_task_return_boolean(task, false);
      return;
    }
  }
  LOG("Sync: Perform sync");
  // Get calendars
  caldav_client_pull_calendars(client);
  LOG("Sync: Loaded %zu calendars", client->calendars->count);
  // Get events
  for (size_t i = 0; i < client->calendars->count; i++) {
    CalDAVCalendar *cal = client->calendars->items[i];
    CONTINUE_IF_NOT(cal->component_set & CALDAV_COMPONENT_SET_VTODO);
    if (cal->events_changed) caldav_calendar_pull_events(cal);
  }

  // Create/Update calendars on server

  g_autoptr(GPtrArray) tasks_to_push_copy =
      g_ptr_array_new_from_array(tasks_to_push->pdata, tasks_to_push->len, NULL, NULL, NULL);

  // Create/Update tasks on server
  for_range(i, 0, tasks_to_push_copy->len) {
    TaskData *data = g_ptr_array_index(tasks_to_push, i);
    autoptr(icalcomponent) wrapper = icalcomponent_new_vcalendar();
    icalcomponent_add_component(wrapper, icalcomponent_new_clone(data->ical));
    const char *uid = errands_data_get_uid(data->ical);
    const char *ical = icalcomponent_as_ical_string(wrapper);
    CalDAVCalendar *cal = find_calendar_by_uid(data->list->uid);
    CalDAVEvent *existing_event = find_event_by_uid(cal, uid);
    if (existing_event) {
      LOG("Updating event on server: %s", uid);
      caldav_event_update(existing_event, ical);
    } else {
      LOG("Creating event on server: %s", uid);
      caldav_calendar_create_event(cal, uid, ical);
    }
    g_ptr_array_add(tasks_pushed, data);
  }

  g_task_return_boolean(task, true);
}

static void errands__sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {
  gtk_widget_set_visible(state.main_window->sidebar->sync_indicator, false);
  // Cleanup synced tasks
  for_range(i, 0, tasks_pushed->len) {
    TaskData *data = g_ptr_array_index(tasks_pushed, i);
    guint idx = 0;
    bool found = g_ptr_array_find(tasks_to_push, data, &idx);
    if (found) g_ptr_array_remove_index_fast(tasks_to_push, idx);
  }
  g_ptr_array_set_size(tasks_pushed, 0);
  if (!g_task_propagate_boolean(G_TASK(res), NULL)) return;

  bool reload = false;
  for_range(i, 0, client->calendars->count) {
    CalDAVCalendar *cal = client->calendars->items[i];
    CONTINUE_IF_NOT(cal->component_set & CALDAV_COMPONENT_SET_VTODO);
    const char *uid = get_calendar_uid_from_href(cal->href);
    ListData *existing_list = errands_data_find_list_data_by_uid(uid);
    if (!existing_list) {
      LOG("Sync: Create new list '%s'", uid);
      reload = true;
      ListData *new_list = errands_list_data_create(uid, cal->display_name, cal->color, false, true);
      g_ptr_array_add(errands_data_lists, new_list);
      // Add tasks
      for_range(j, 0, cal->events->count) {
        CalDAVEvent *e = cal->events->items[j];
        icalcomponent *event = icalcomponent_new_from_string(e->ical);
        CONTINUE_IF_NOT(event);
        if (icalcomponent_isa(event) == ICAL_VCALENDAR_COMPONENT) {
          icalcomponent *inner = icalcomponent_get_first_component(event, ICAL_VTODO_COMPONENT);
          CONTINUE_IF_NOT(inner);
          const char *parent = errands_data_get_parent(inner);
          TaskData *parent_task = NULL;
          if (parent) parent_task = errands_task_data_find_by_uid(new_list, parent);
          errands_task_data_new(inner, parent_task, new_list);
        }
      }
    }
    // Merge calendar data into existing list or create a new one
    // if (existing_list) {
    //   icalcomponent_merge_component(existing_list->ical, cal->ical);
    // }
  }

  if (reload) {
    errands_sidebar_load_lists();
    errands_task_list_reload(state.main_window->task_list, false);
  }
  sync_in_progress = false;
  sync_scheduled = false;
  LOG("Sync finished");
}

static bool errands__sync() {
  if (!sync_scheduled) return true;
  if (sync_in_progress) {
    LOG("Sync is in progress. Skipping.");
    return true;
  }
  sync_in_progress = true;
  gtk_widget_set_visible(state.main_window->sidebar->sync_indicator, true);
  RUN_THREAD_FUNC(errands__sync_cb, errands__sync_finished_cb);
  return true;
}

void errands_sync_init(void) {
  tasks_to_push = g_ptr_array_new_full(16, NULL);
  tasks_pushed = g_ptr_array_new_full(16, NULL);
  gtk_widget_set_visible(state.main_window->sidebar->sync_indicator, true);
  RUN_THREAD_FUNC(errands__sync_cb, errands__sync_finished_cb);
  g_timeout_add_seconds(10, G_SOURCE_FUNC(errands__sync), NULL);
}

void errands_sync_cleanup(void) {
  if (client) caldav_client_free(client);
  if (tasks_to_push) g_ptr_array_free(tasks_to_push, true);
  if (tasks_pushed) g_ptr_array_free(tasks_pushed, true);
}

void errands_sync_schedule(void) { sync_scheduled = true; }

void errands_sync_schedule_list(ListData *data) {
  sync_scheduled = true;
  list_data = data;
}

void errands_sync_schedule_task(TaskData *data) {
  sync_scheduled = true;
  g_ptr_array_add(tasks_to_push, data);
}
