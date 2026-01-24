#include "sync.h"
#include "data.h"
#include "glib.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "window.h"
#include <libical/ical.h>
#include <unistd.h>

#define CALDAV_DEBUG
#define CALDAV_IMPLEMENTATION
#include "vendor/caldav.h"
#include "vendor/toolbox.h"

#include <glib/gi18n.h>

#include <stdatomic.h>

static atomic_bool sync_initialized = false;
static bool sync_in_progress = false;
static bool sync_again = false;

static CalDAVClient *client = NULL;

typedef enum {
  LISTS_TO_DELETE_LOCAL,
  LISTS_TO_DELETE,
  LISTS_TO_DELETE_COPY,
  LISTS_DELETED,

  LISTS_TO_CREATE,
  LISTS_TO_CREATE_COPY,
  LISTS_CREATED,

  LISTS_TO_UPDATE,
  LISTS_TO_UPDATE_COPY,
  LISTS_UPDATED,

  TASKS_TO_DELETE,
  TASKS_TO_DELETE_COPY,
  TASKS_DELETED,

  TASKS_TO_CREATE,
  TASKS_TO_CREATE_COPY,
  TASKS_CREATED,

  TASKS_TO_UPDATE,
  TASKS_TO_UPDATE_COPY,
  TASKS_UPDATED,

  ERRANDS_SYNC_LIST_TYPE_N
} ErrandsSyncListType;

static GPtrArray *lists[ERRANDS_SYNC_LIST_TYPE_N] = {0};

// --- UTILS --- //

static bool calendar_is_vtodo(CalDAVCalendar *c) {
  if (c->component_set & CALDAV_COMPONENT_SET_VFREEBUSY) return false;
  if (c->component_set & CALDAV_COMPONENT_SET_VJOURNAL) return false;
  if (c->component_set & CALDAV_COMPONENT_SET_VEVENT) return false;
  return true;
}

static CalDAVCalendar *find_calendar_by_uid(const char *uid) {
  for_range(i, 0, client->calendars->count) {
    CalDAVCalendar *c = client->calendars->items[i];
    CONTINUE_IF_NOT(calendar_is_vtodo(c));
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

static bool task_uid_exists(const char *uid) {
  for_range(i, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, i);
    for (icalcomponent *c = icalcomponent_get_first_component(list->ical, ICAL_VTODO_COMPONENT); c != 0;
         c = icalcomponent_get_next_component(list->ical, ICAL_VTODO_COMPONENT))
      if (STR_EQUAL(uid, icalcomponent_get_uid(c))) return true;
  }
  return false;
}

// --- SYNC --- //

static bool errands__sync_init(void) {
  // Check if sync is disabled.
  if (!errands_settings_get(SETTING_SYNC).b) return false;
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

// Pulls calendars and events and pushing changes to the server.
// Runs in separate thread to not block the UI.
static void errands__sync_cb(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable) {
  if (!sync_initialized)
    if (!errands__sync_init()) {
      g_task_return_boolean(task, false);
      return;
    }

  // Get calendars from the server
  caldav_client_pull_calendars(client);

  // Get lists deleted on server while the app was not running
  for_range(i, 0, errands_data_lists->len) {
    ListData *l = g_ptr_array_index(errands_data_lists, i);
    CONTINUE_IF_NOT(errands_data_get_synced(l->ical));
    CONTINUE_IF(errands_data_get_deleted(l->ical));
    if (!find_calendar_by_uid(l->uid)) {
      LOG("Sync: List was deleted while app was not running: %s", l->uid);
      g_ptr_array_add(lists[LISTS_TO_DELETE_LOCAL], l);
    }
  }

  // Update events and get deleted lists
  for_range(i, 0, client->calendars->count) {
    CalDAVCalendar *c = client->calendars->items[i];
    CONTINUE_IF_NOT(calendar_is_vtodo(c));
    // Get lists deleted on server
    if (c->deleted) {
      ListData *list = errands_data_find_list_data_by_uid(c->uid);
      if (list) {
        LOG("Sync: List was deleted on server: %s", c->uid);
        g_ptr_array_add(lists[LISTS_TO_DELETE_LOCAL], list);
      }
    }
    // Get events
    if (c->events_changed) caldav_calendar_pull_events(c);
  }

  // Delete lists on server
  for_range(i, 0, lists[LISTS_TO_DELETE_COPY]->len) {
    ListData *list = g_ptr_array_index(lists[LISTS_TO_DELETE_COPY], i);
    CalDAVCalendar *c = find_calendar_by_uid(list->uid);
    CONTINUE_IF_NOT(c);
    LOG("Sync: Deleting calendar on server: %s", list->uid);
    errands_data_set_synced(list->ical, caldav_calendar_delete(c));
    errands_list_data_save(list);
    g_ptr_array_add(lists[LISTS_DELETED], list);
  }
  g_ptr_array_set_size(lists[LISTS_TO_DELETE_COPY], 0);

  // Create lists on server
  for_range(i, 0, lists[LISTS_TO_CREATE_COPY]->len) {
    ListData *list = g_ptr_array_index(lists[LISTS_TO_CREATE_COPY], i);
    CalDAVCalendar *c = find_calendar_by_uid(list->uid);
    CONTINUE_IF(c);
    const char *name = errands_data_get_list_name(list->ical);
    const char *color = errands_data_get_color(list->ical, true);
    bool created = caldav_client_create_calendar(client, list->uid, name, NULL, color, CALDAV_COMPONENT_SET_VTODO);
    if (created) {
      LOG("Sync: Created calendar on server: %s", list->uid);
      caldav_client_pull_calendars(client);
      g_autoptr(GPtrArray) tasks = g_ptr_array_sized_new(list->children->len);
      errands_data_get_flat_list(tasks);
      g_ptr_array_extend(lists[TASKS_TO_CREATE_COPY], tasks, NULL, NULL);
      errands_data_set_synced(list->ical, true);
      errands_list_data_save(list);
    }
    g_ptr_array_add(lists[LISTS_CREATED], list);
  }
  g_ptr_array_set_size(lists[LISTS_TO_CREATE_COPY], 0);

  // Update lists on server
  for_range(i, 0, lists[LISTS_TO_UPDATE_COPY]->len) {
    ListData *list = g_ptr_array_index(lists[LISTS_TO_UPDATE_COPY], i);
    CONTINUE_IF(errands_data_get_deleted(list->ical));
    CalDAVCalendar *c = find_calendar_by_uid(list->uid);
    CONTINUE_IF(!c || errands_data_get_synced(list->ical));
    const char *name = errands_data_get_list_name(list->ical);
    const char *color = errands_data_get_color(list->ical, true);
    if (!STR_EQUAL(name, c->display_name) || !STR_EQUAL(color, c->color)) {
      LOG("Sync: Updating list properties on server: %s", list->uid);
      caldav_calendar_update(c, name, NULL, color);
      errands_data_set_synced(list->ical, true);
      errands_list_data_save(list);
    }
    g_ptr_array_add(lists[LISTS_UPDATED], list);
  }
  g_ptr_array_set_size(lists[LISTS_TO_UPDATE_COPY], 0);

  // Delete tasks on server
  for_range(i, 0, lists[TASKS_TO_DELETE_COPY]->len) {
    TaskData *task = g_ptr_array_index(lists[TASKS_TO_DELETE_COPY], i);
    CalDAVCalendar *c = find_calendar_by_uid(task->list->uid);
    CONTINUE_IF_NOT(c);
    const char *uid = errands_data_get_uid(task->ical);
    CalDAVEvent *e = find_event_by_uid(c, uid);
    CONTINUE_IF_NOT(e);
    LOG("Sync: Deleting task on server: %s", uid);
    caldav_event_delete(e);
    g_ptr_array_add(lists[TASKS_DELETED], task);
  }
  g_ptr_array_set_size(lists[TASKS_TO_DELETE_COPY], 0);

  // Create tasks on server
  for_range(i, 0, lists[TASKS_TO_CREATE_COPY]->len) {
    TaskData *task = g_ptr_array_index(lists[TASKS_TO_CREATE_COPY], i);
    CalDAVCalendar *c = find_calendar_by_uid(task->list->uid);
    CONTINUE_IF_NOT(c);
    const char *uid = errands_data_get_uid(task->ical);
    CalDAVEvent *e = find_event_by_uid(c, uid);
    CONTINUE_IF(e);
    LOG("Sync: Creating event on server: %s", uid);
    autoptr(icalcomponent) ical = icalcomponent_new_vcalendar();
    icalcomponent_add_component(ical, icalcomponent_new_clone(task->ical));
    caldav_calendar_create_event(c, uid, icalcomponent_as_ical_string(ical));
    g_ptr_array_add(lists[TASKS_CREATED], task);
  }
  g_ptr_array_set_size(lists[TASKS_TO_CREATE_COPY], 0);

  // Update tasks on server
  // for_range(i, 0, lists[TASKS_TO_UPDATE_COPY]->len) {
  //   TaskData *task = g_ptr_array_index(lists[TASKS_TO_UPDATE_COPY], i);
  //   CalDAVCalendar *c = find_calendar_by_uid(task->list->uid);
  //   CONTINUE_IF_NOT(c);
  //   const char *uid = errands_data_get_uid(task->ical);
  //   CalDAVEvent *e = find_event_by_uid(c, uid);
  //   CONTINUE_IF_NOT(e);
  //   LOG("Sync: Updating event on server: %s", uid);
  //   autoptr(icalcomponent) ical = icalcomponent_new_vcalendar();
  //   icalcomponent_add_component(ical, icalcomponent_new_clone(task->ical));
  //   caldav_event_update(e, icalcomponent_as_ical_string(ical));
  //   g_ptr_array_add(lists[TASKS_UPDATED], task);
  // }
  // g_ptr_array_set_size(lists[TASKS_TO_UPDATE_COPY], 0);

  g_task_return_boolean(task, true);
}

// Runs in main thread on sync finish
static void errands__sync_finished_cb(GObject *source_object, GAsyncResult *res, gpointer user_data) {

#define errands__cleanup_list(with, from)                                                                              \
  for_range(i, 0, lists[with]->len) g_ptr_array_remove_fast(lists[from], g_ptr_array_index(lists[with], i));           \
  g_ptr_array_set_size(lists[with], 0)

  errands_sidebar_toggle_sync_indicator(false);
  if (!g_task_propagate_boolean(G_TASK(res), NULL)) return;

  errands__cleanup_list(LISTS_DELETED, LISTS_TO_DELETE);
  errands__cleanup_list(LISTS_CREATED, LISTS_TO_CREATE);
  errands__cleanup_list(LISTS_UPDATED, LISTS_TO_UPDATE);
  errands__cleanup_list(TASKS_DELETED, TASKS_TO_DELETE);
  errands__cleanup_list(TASKS_CREATED, TASKS_TO_CREATE);
  errands__cleanup_list(TASKS_UPDATED, TASKS_TO_UPDATE);

  bool reload = false;

  // Delete local lists
  for_range(i, 0, lists[LISTS_TO_DELETE_LOCAL]->len) {
    ListData *list = g_ptr_array_index(lists[LISTS_TO_DELETE_LOCAL], i);
    errands_data_set_deleted(list->ical, true);
    errands_data_set_synced(list->ical, true);
    errands_list_data_save(list);
    const char *msg =
        tmp_str_printf("%s: %s", _("Task List was deleted on server"), errands_data_get_list_name(list->ical));
    errands_window_add_toast(msg);
    reload = true;
  }
  g_ptr_array_set_size(lists[LISTS_TO_DELETE_LOCAL], 0);

  // Create local lists
  for_range(i, 0, client->calendars->count) {
    CalDAVCalendar *c = client->calendars->items[i];
    CONTINUE_IF_NOT(calendar_is_vtodo(c));
    ListData *list = errands_data_find_list_data_by_uid(c->uid);
    if (!list) {
      LOG("Sync: Create new local list: %s", c->uid);
      icalcomponent *ical = caldav_calendar_to_icalcomponent(c);
      ListData *new_list = errands_list_data_load_from_ical(ical, c->uid, c->display_name, c->color);
      g_ptr_array_add(errands_data_lists, new_list);
      errands_list_data_save(new_list);
      reload = true;
    }
  }

  // Update local list props
  for_range(i, 0, client->calendars->count) {
    CalDAVCalendar *c = client->calendars->items[i];
    CONTINUE_IF_NOT(calendar_is_vtodo(c));
    ListData *list = errands_data_find_list_data_by_uid(c->uid);
    CONTINUE_IF_NOT(list);
    bool props_changed = false;
    const char *curr_name = errands_data_get_list_name(list->ical);
    const char *curr_color = errands_data_get_color(list->ical, true);
    if (!STR_EQUAL(curr_name, c->display_name)) {
      errands_data_set_list_name(list->ical, c->display_name);
      props_changed = true;
    }
    if (!STR_EQUAL(curr_color, c->color)) {
      errands_data_set_color(list->ical, c->color, true);
      props_changed = true;
    }
    if (props_changed) {
      LOG("Sync: Update local list properties: %s", c->uid);
      const char *msg =
          tmp_str_printf("%s: %s", _("Task List was updated on server"), errands_data_get_list_name(list->ical));
      errands_window_add_toast(msg);
      errands_list_data_save(list);
      reload = true;
    }
  }

  // TODO: probably hash map needed here
  // Create local tasks
  for_range(i, 0, client->calendars->count) {
    CalDAVCalendar *c = client->calendars->items[i];
    CONTINUE_IF_NOT(calendar_is_vtodo(c));
    ListData *list = errands_data_find_list_data_by_uid(c->uid);
    CONTINUE_IF_NOT(list);
    for_range(j, 0, c->events->count) {
      CalDAVEvent *e = c->events->items[j];
      autoptr(icalcomponent) vcalendar = icalcomponent_new_from_string(e->ical);
      CONTINUE_IF_NOT(vcalendar);
      icalcomponent *vtodo = icalcomponent_get_first_component(vcalendar, ICAL_VTODO_COMPONENT);
      CONTINUE_IF_NOT(vtodo);
      const char *task_uid = icalcomponent_get_uid(vtodo);
      CONTINUE_IF(task_uid_exists(task_uid));
      icalcomponent *clone = icalcomponent_new_clone(vtodo);
      icalcomponent_add_component(list->ical, clone);
      const char *parent_uid = errands_data_get_parent(vtodo);
      // TODO: use parent_uid to add task to a parent
      CONTINUE_IF(parent_uid);
      LOG("Sync: Create local task: %s", task_uid);
      TaskData *new_task = errands_task_data_new(clone, NULL, list);
      errands_list_data_save(list);
      reload = true;
    }
  }

  if (reload) {
    errands_sidebar_load_lists();
    errands_task_list_reload(state.main_window->task_list, false);
  }

  sync_in_progress = false;
  LOG("Sync: Finished");
  if (sync_again) {
    sync_again = false;
    errands_sync();
  }
}

bool errands_sync() {
  if (!errands_settings_get(SETTING_SYNC).b) return false;

  if (sync_in_progress) {
    LOG("Sync: In progress");
    sync_again = true;
    return true;
  }
  LOG("Sync: Started");

  g_ptr_array_extend(lists[LISTS_TO_DELETE_COPY], lists[LISTS_TO_DELETE], NULL, NULL);
  g_ptr_array_extend(lists[LISTS_TO_CREATE_COPY], lists[LISTS_TO_CREATE], NULL, NULL);
  g_ptr_array_extend(lists[LISTS_TO_UPDATE_COPY], lists[LISTS_TO_UPDATE], NULL, NULL);

  g_ptr_array_extend(lists[TASKS_TO_DELETE_COPY], lists[TASKS_TO_DELETE], NULL, NULL);
  g_ptr_array_extend(lists[TASKS_TO_CREATE_COPY], lists[TASKS_TO_CREATE], NULL, NULL);
  g_ptr_array_extend(lists[TASKS_TO_UPDATE_COPY], lists[TASKS_TO_UPDATE], NULL, NULL);

  errands_sidebar_toggle_sync_indicator(true);
  sync_in_progress = true;
  g_autoptr(GTask) task = g_task_new(NULL, NULL, errands__sync_finished_cb, NULL);
  g_task_run_in_thread(task, errands__sync_cb);

  return true;
}

void errands_sync_init(void) {
  LOG("Sync: Initialize");
  for_range(i, 0, ERRANDS_SYNC_LIST_TYPE_N) lists[i] = g_ptr_array_sized_new(8);

  g_autoptr(GTask) task = g_task_new(NULL, NULL, errands__sync_finished_cb, NULL);
  g_task_run_in_thread(task, errands__sync_cb);

  int interval = errands_settings_get(SETTING_SYNC_INTERVAL).i;
  g_timeout_add_seconds(interval >= 10 ? interval : 10, G_SOURCE_FUNC(errands_sync), NULL);
}

void errands_sync_delete_list(ListData *data) {
  g_ptr_array_add(lists[LISTS_TO_DELETE], data);
  errands_sync();
}

void errands_sync_create_list(ListData *data) {
  g_ptr_array_add(lists[LISTS_TO_CREATE], data);
  errands_sync();
}

void errands_sync_update_list(ListData *data) {
  g_ptr_array_add(lists[LISTS_TO_UPDATE], data);
  errands_sync();
}

void errands_sync_delete_task(TaskData *data) {
  g_ptr_array_add(lists[TASKS_TO_DELETE], data);
  errands_sync();
}

void errands_sync_create_task(TaskData *data) {
  g_ptr_array_add(lists[TASKS_TO_CREATE], data);
  errands_sync();
}

void errands_sync_update_task(TaskData *data) {
  g_ptr_array_add(lists[TASKS_TO_UPDATE], data);
  errands_sync();
}

void errands_sync_cleanup(void) {
  LOG("Sync: Cleanup");
  if (client) caldav_client_free(client);
  for_range(i, 0, ERRANDS_SYNC_LIST_TYPE_N) g_ptr_array_free(lists[i], true);
}
