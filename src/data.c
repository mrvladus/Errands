#include "data.h"
#include "state.h"
#include "utils.h"

#include "lib/cJSON.h"
#include "utils/files.h"
#include "utils/macros.h"

#include <glib.h>
#include <libical/ical.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define PATH_SEP "/"
const char *user_dir;

// --- ICAL UTILS --- //

// Get X property
icalproperty *__get_x_prop(icalcomponent *ical, const char *xprop) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_X_PROPERTY);
  while (property) {
    const char *name = icalproperty_get_x_name(property);
    if (name && !strcmp(name, xprop))
      return property;
    property = icalcomponent_get_next_property(ical, ICAL_X_PROPERTY);
  }
  return NULL;
}

// Get X property value string
const char *__get_x_prop_value(icalcomponent *ical, const char *xprop) {
  icalproperty *property = __get_x_prop(ical, xprop);
  if (property)
    return icalproperty_get_value_as_string(property);
  return NULL;
}

// Add X property value string
void __add_x_prop(icalcomponent *ical, const char *xprop, const char *value) {
  icalproperty *property = icalproperty_new_x(value);
  if (!property)
    return;
  icalproperty_set_x_name(property, xprop);
  icalcomponent_add_property(ical, property);
}

bool __calendar_get_x_bool(icalcomponent *ical, const char *prop) {
  const char *property = __get_x_prop_value(ical, prop);
  if (property)
    return (bool)atoi(property);
  else
    __add_x_prop(ical, prop, "0");
  return false;
}

const char *__calendar_get_string(icalcomponent *ical, const char *prop) {
  const char *property = __get_x_prop_value(ical, prop);
  if (property)
    return property;
  else
    return NULL;
}

void calendar_set_bool(icalcomponent *ical, const char *prop, bool value) {}

// --- CALENDAR --- //

CalendarData calendar_data_new(const char *uid, icalcomponent *ical) {
  CalendarData data;
  data.uid = strdup(uid);
  data.ical = ical;
  return data;
}

void calendar_data_free(CalendarData *data) {
  icalcomponent_free(data->ical);
  free(data->uid);
}

const char *calendar_data_get_uid(CalendarData *data) { return data->uid; }

const char *calendar_data_get_name(CalendarData *data) {
  return __get_x_prop_value(data->ical, "X-WR-CALNAME");
}

const char *calendar_data_get_color(CalendarData *data) {
  return __get_x_prop_value(data->ical, "X-ERRANDS-COLOR");
}

bool calendar_data_get_deleted(CalendarData *data) {
  return __calendar_get_x_bool(data->ical, "X-ERRANDS-DELETED");
}

bool calendar_data_get_synced(CalendarData *data) {
  return __calendar_get_x_bool(data->ical, "X-ERRANDS-SYNCED");
}

// --- EVENT --- //

EventData event_data_new(icalcomponent *ical) {
  EventData data;
  data.ical = ical;
  return data;
}

void event_data_free(EventData *data) { icalcomponent_free(data->ical); }

// const char *event_data_get_attachments(EventData *data);
// const char *event_data_get_color(EventData *data);
// bool event_data_get_completed(EventData *data);
// const char *event_data_get_changed_at(EventData *data);
// const char *event_data_get_created_at(EventData *data);
// bool event_data_get_deleted(EventData *data);
// const char *event_data_get_due_date(EventData *data);
// bool event_data_get_expanded(EventData *data);
// const char *event_data_get_list_uid(EventData *data);
// const char *event_data_get_notes(EventData *data);
// bool event_data_get_notified(EventData *data);
// const char *event_data_get_parent(EventData *data);
// uint8_t event_data_get_percent(EventData *data);
// uint8_t event_data_get_priority(EventData *data);
// const char *event_data_get_rrule(EventData *data); // ?
// const char *event_data_get_start_date(EventData *data);
// bool event_data_get_synced(EventData *data);
// const char *event_data_get_tags(EventData *data);
const char *event_data_get_text(EventData *data) { return icalcomponent_get_summary(data->ical); }
void event_data_set_text(EventData *data, const char *text) {
  icalcomponent_set_summary(data->ical, text);
}
// bool event_data_get_toolbar_shown(EventData *data);
// bool event_data_get_trash(EventData *data);
// const char *event_data_get_uid(EventData *data);

// --- LOADING --- //

static void errands_data_migrate() {
  g_autofree gchar *old_data_file = g_build_path(PATH_SEP, user_dir, "data.json", NULL);
  if (!file_exists(old_data_file))
    return;
  LOG("Migrate user data");
  char *json_data = read_file_to_string(old_data_file);
  cJSON *json = cJSON_Parse(json_data);
  free(json_data);
  if (!json)
    return;
  cJSON *cal_arr = cJSON_GetObjectItem(json, "lists");
  cJSON *cal_item;
  for (size_t i = 0; i < cJSON_GetArraySize(cal_arr); i++) {
    cal_item = cJSON_GetArrayItem(cal_arr, i);
    char *color = cJSON_GetObjectItem(cal_item, "color")->valuestring;
    bool deleted = (bool)cJSON_GetObjectItem(cal_item, "deleted")->valueint;
    bool synced = (bool)cJSON_GetObjectItem(cal_item, "synced")->valueint;
    char *name = cJSON_GetObjectItem(cal_item, "name")->valuestring;
    char *list_uid = cJSON_GetObjectItem(cal_item, "uid")->valuestring;

    icalcomponent *calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
    icalcomponent_add_property(calendar, icalproperty_new_version("2.0"));
    icalcomponent_add_property(calendar, icalproperty_new_prodid("~//Errands"));
    __add_x_prop(calendar, "X-WR-CALNAME", name);
    __add_x_prop(calendar, "X-ERRANDS-COLOR", color);
    __add_x_prop(calendar, "X-ERRANDS-DELETED", deleted ? "1" : "0");
    __add_x_prop(calendar, "X-ERRANDS-SYNCED", synced ? "1" : "0");
    // Add events
    cJSON *tasks_arr = cJSON_GetObjectItem(json, "tasks");
    cJSON *task_item;
    for (size_t j = 0; j < cJSON_GetArraySize(tasks_arr); j++) {
      task_item = cJSON_GetArrayItem(tasks_arr, j);
      char *task_list_uid = cJSON_GetObjectItem(task_item, "list_uid")->valuestring;
      if (strcmp(task_list_uid, list_uid))
        continue;
      cJSON *task_attachments_arr = cJSON_GetObjectItem(task_item, "attachments");
      g_autoptr(GString) attachments = g_string_new("");
      size_t attachments_arr_size = cJSON_GetArraySize(task_attachments_arr);
      for (size_t a = 0; a < attachments_arr_size; a++) {
        char *attachment_str = cJSON_GetArrayItem(task_attachments_arr, a)->valuestring;
        g_string_append(attachments, attachment_str);
        if (a > 0 && a < attachments_arr_size)
          g_string_append(attachments, ",");
      }
      char *color = cJSON_GetObjectItem(task_item, "color")->valuestring;
      bool completed = (bool)cJSON_GetObjectItem(task_item, "completed")->valueint;
      char *changed_at = cJSON_GetObjectItem(task_item, "changed_at")->valuestring;
      char *created_at = cJSON_GetObjectItem(task_item, "created_at")->valuestring;
      bool deleted = (bool)cJSON_GetObjectItem(task_item, "deleted")->valueint;
      char *due_date = cJSON_GetObjectItem(task_item, "due_date")->valuestring;
      bool expanded = (bool)cJSON_GetObjectItem(task_item, "expanded")->valueint;
      char *notes = cJSON_GetObjectItem(task_item, "notes")->valuestring;
      bool notified = (bool)cJSON_GetObjectItem(task_item, "notified")->valueint;
      char *parent = cJSON_GetObjectItem(task_item, "parent")->valuestring;
      uint8_t percent_complete = cJSON_GetObjectItem(task_item, "percent_complete")->valueint;
      uint8_t priority = cJSON_GetObjectItem(task_item, "priority")->valueint;
      char *rrule = cJSON_GetObjectItem(task_item, "rrule")->valuestring;
      char *start_date = cJSON_GetObjectItem(task_item, "start_date")->valuestring;
      bool synced = (bool)cJSON_GetObjectItem(task_item, "synced")->valueint;
      cJSON *task_tags_arr = cJSON_GetObjectItem(task_item, "attachments");
      g_autoptr(GString) tags = g_string_new("");
      size_t tags_arr_size = cJSON_GetArraySize(task_tags_arr);
      for (size_t a = 0; a < tags_arr_size; a++) {
        char *tag_str = cJSON_GetArrayItem(task_tags_arr, a)->valuestring;
        g_string_append(tags, tag_str);
        if (a > 0 && a < tags_arr_size)
          g_string_append(tags, ",");
      }
      char *text = cJSON_GetObjectItem(task_item, "text")->valuestring;
      bool toolbar_shown = (bool)cJSON_GetObjectItem(task_item, "toolbar_shown")->valueint;
      bool trash = (bool)cJSON_GetObjectItem(task_item, "trash")->valueint;
      char *uid = cJSON_GetObjectItem(task_item, "uid")->valuestring;

      icalcomponent *event = icalcomponent_new(ICAL_VTODO_COMPONENT);
      if (completed)
        icalcomponent_add_property(event, icalproperty_new_completed(icaltime_today()));
      if (strcmp(tags->str, ""))
        icalcomponent_add_property(event, icalproperty_new_categories(tags->str));
      icalcomponent_add_property(event,
                                 icalproperty_new_lastmodified(icaltime_from_string(changed_at)));
      icalcomponent_add_property(event, icalproperty_new_dtstamp(icaltime_from_string(created_at)));
      if (strcmp(due_date, ""))
        icalcomponent_add_property(event, icalproperty_new_due(icaltime_from_string(due_date)));
      if (strcmp(notes, ""))
        icalcomponent_add_property(event, icalproperty_new_description(notes));
      if (strcmp(parent, ""))
        icalcomponent_add_property(event, icalproperty_new_relatedto(parent));
      icalcomponent_add_property(event, icalproperty_new_percentcomplete(percent_complete));
      icalcomponent_add_property(event, icalproperty_new_priority(priority));
      if (strcmp(rrule, ""))
        icalcomponent_add_property(event,
                                   icalproperty_new_rrule(icalrecurrencetype_from_string(rrule)));
      if (strcmp(start_date, ""))
        icalcomponent_add_property(event,
                                   icalproperty_new_dtstart(icaltime_from_string(start_date)));
      icalcomponent_add_property(event, icalproperty_new_summary(text));
      icalcomponent_add_property(event, icalproperty_new_uid(uid));
      __add_x_prop(event, "X-ERRANDS-ATTACHMENTS", attachments->str);
      __add_x_prop(event, "X-ERRANDS-COLOR", color);
      __add_x_prop(event, "X-ERRANDS-DELETED", deleted ? "1" : "0");
      __add_x_prop(event, "X-ERRANDS-EXPANDED", expanded ? "1" : "0");
      __add_x_prop(event, "X-ERRANDS-NOTIFIED", notified ? "1" : "0");
      __add_x_prop(event, "X-ERRANDS-SYNCED", synced ? "1" : "0");
      __add_x_prop(event, "X-ERRANDS-TOOLBAR-SHOWN", toolbar_shown ? "1" : "0");
      __add_x_prop(event, "X-ERRANDS-TRASH", trash ? "1" : "0");
      __add_x_prop(event, "X-ERRANDS-LIST-UID", task_list_uid);

      icalcomponent_add_component(calendar, event);
    }
    g_autofree gchar *calendar_filename = g_strdup_printf("%s.ics", list_uid);
    g_autofree gchar *calendar_file_path =
        g_build_path(PATH_SEP, user_dir, calendar_filename, NULL);
    write_string_to_file(calendar_file_path, icalcomponent_as_ical_string(calendar));
  }
  cJSON_Delete(json);
  // TODO: Delete data.json
}

CalendarData_array errands_data_load_calendars() {
  user_dir = g_build_path(PATH_SEP, g_get_user_data_dir(), "errands", NULL);
  if (!directory_exists(user_dir)) {
    LOG("Creating user data directory at %s", user_dir);
    g_mkdir_with_parents(user_dir, 0755);
  }
  LOG("Loading user data at %s", user_dir);
  errands_data_migrate();
  CalendarData_array array = calendar_data_array_new();
  g_autoptr(GDir) dir = g_dir_open(user_dir, 0, NULL);
  if (!dir)
    return array;
  const char *filename;
  while ((filename = g_dir_read_name(dir))) {
    char *is_ics = strstr(filename, ".ics");
    if (is_ics) {
      g_autofree gchar *path = g_build_path(PATH_SEP, user_dir, filename, NULL);
      char *content = read_file_to_string(path);
      if (content) {
        LOG("Loading calendar %s", path);
        icalcomponent *calendar = icalparser_parse_string(content);
        if (calendar) {
          *(strstr(filename, ".")) = '\0';
          calendar_data_array_append(&array, calendar_data_new(filename, calendar));
        }
        free(content);
      }
    }
  }
  return array;
}

EventData_array errands_data_load_events(CalendarData_array *calendars) {
  EventData_array events = event_data_array_new();
  for (size_t i = 0; i < calendars->len; i++) {
    CalendarData calendar = calendars->data[i];
    icalcomponent *c;
    for (c = icalcomponent_get_first_component(calendar.ical, ICAL_VTODO_COMPONENT); c != 0;
         c = icalcomponent_get_next_component(calendar.ical, ICAL_VTODO_COMPONENT))
      event_data_array_append(&events, event_data_new(c));
  }
  return events;
}

// --- READ / WRITE --- //

// Function to validate JSON data
static bool validate_json(const char *json_data) {
  cJSON *json = cJSON_Parse(json_data);
  if (json == NULL)
    return false;
  cJSON_Delete(json);
  return true;
}

// Function to read a data.json file
static char *errands_data_read() {
  const char *data_dir = g_build_path("/", g_get_user_data_dir(), "errands", NULL);
  if (!directory_exists(data_dir))
    g_mkdir_with_parents(data_dir, 0755);
  const char *data_file_path = g_build_path("/", data_dir, "data.json", NULL);
  if (!file_exists(data_file_path)) {
    FILE *file = fopen(data_file_path, "w");
    fprintf(file, "{\"lists\":[],\"tags\":[],\"tasks\":[]}");
    fclose(file);
  } else {
    // create_backup(data_file_path);
  }
  char *json_data = read_file_to_string(data_file_path);
  if (json_data == NULL) {
    g_free((gpointer)data_dir);
    g_free((gpointer)data_file_path);
    return NULL;
  }
  if (!validate_json(json_data)) {
    free(json_data);
    g_free((gpointer)data_dir);
    g_free((gpointer)data_file_path);
    return NULL;
  }
  g_free((gpointer)data_dir);
  g_free((gpointer)data_file_path);
  return json_data;
}

// Load user data from data.json
void errands_data_load() {
  LOG("Loading user data");
  char *data = errands_data_read();
  if (!data)
    return;

  cJSON *json = cJSON_Parse(data);
  free(data);

  // Parse lists
  state.tl_data = g_ptr_array_new();
  cJSON *arr = cJSON_GetObjectItem(json, "lists");
  int len = cJSON_GetArraySize(arr);
  cJSON *item;
  for (int i = 0; i < len; i++) {
    item = cJSON_GetArrayItem(arr, i);
    // Check if item deleted
    // TODO: skip if sync enabled and item synced
    if (cJSON_GetObjectItem(item, "deleted")->valueint)
      continue;
    TaskListData *tl = malloc(sizeof(*tl));
    strcpy(tl->color, cJSON_GetObjectItem(item, "color")->valuestring);
    tl->deleted = (bool)cJSON_GetObjectItem(item, "deleted")->valueint;
    strncpy(tl->name, cJSON_GetObjectItem(item, "name")->valuestring, 255);
    tl->show_completed = (bool)cJSON_GetObjectItem(item, "show_completed")->valueint;
    tl->synced = (bool)cJSON_GetObjectItem(item, "synced")->valueint;
    strcpy(tl->uid, cJSON_GetObjectItem(item, "uid")->valuestring);
    g_ptr_array_add(state.tl_data, tl);
  }

  // Parse tasks
  arr = cJSON_GetObjectItem(json, "tasks");
  len = cJSON_GetArraySize(arr);
  state.t_data = g_ptr_array_new();
  for (int i = 0; i < len; i++) {
    item = cJSON_GetArrayItem(arr, i);
    // Check if item deleted
    // TODO: skip if sync enabled and item synced
    if (cJSON_GetObjectItem(item, "deleted")->valueint)
      continue;
    TaskData *t = malloc(sizeof(*t));
    // Get attachments
    t->attachments = g_ptr_array_new();
    cJSON *atch_arr = cJSON_GetObjectItem(item, "attachments");
    for (int i = 0; i < cJSON_GetArraySize(atch_arr); i++)
      g_ptr_array_add(t->attachments, strdup(cJSON_GetArrayItem(atch_arr, i)->valuestring));
    strcpy(t->color, cJSON_GetObjectItem(item, "color")->valuestring);
    t->completed = (bool)cJSON_GetObjectItem(item, "completed")->valueint;
    strcpy(t->changed_at, cJSON_GetObjectItem(item, "changed_at")->valuestring);
    strcpy(t->created_at, cJSON_GetObjectItem(item, "created_at")->valuestring);
    t->deleted = (bool)cJSON_GetObjectItem(item, "deleted")->valueint;
    strcpy(t->due_date, cJSON_GetObjectItem(item, "due_date")->valuestring);
    t->expanded = (bool)cJSON_GetObjectItem(item, "expanded")->valueint;
    strcpy(t->list_uid, cJSON_GetObjectItem(item, "list_uid")->valuestring);
    t->notes = strdup(cJSON_GetObjectItem(item, "notes")->valuestring);
    t->notified = (bool)cJSON_GetObjectItem(item, "notified")->valueint;
    strcpy(t->parent, cJSON_GetObjectItem(item, "parent")->valuestring);
    t->percent_complete = cJSON_GetObjectItem(item, "percent_complete")->valueint;
    t->priority = cJSON_GetObjectItem(item, "priority")->valueint;
    t->rrule = strdup(cJSON_GetObjectItem(item, "rrule")->valuestring);
    strcpy(t->start_date, cJSON_GetObjectItem(item, "start_date")->valuestring);
    t->synced = (bool)cJSON_GetObjectItem(item, "synced")->valueint;
    // Get tags
    t->tags = g_ptr_array_new();
    cJSON *tags_arr = cJSON_GetObjectItem(item, "tags");
    for (int i = 0; i < cJSON_GetArraySize(tags_arr); i++)
      g_ptr_array_add(t->tags, strdup(cJSON_GetArrayItem(tags_arr, i)->valuestring));
    t->text = strdup(cJSON_GetObjectItem(item, "text")->valuestring);
    t->toolbar_shown = (bool)cJSON_GetObjectItem(item, "toolbar_shown")->valueint;
    t->trash = (bool)cJSON_GetObjectItem(item, "trash")->valueint;
    strcpy(t->uid, cJSON_GetObjectItem(item, "uid")->valuestring);
    g_ptr_array_add(state.t_data, t);
  }

  // Parse tags
  arr = cJSON_GetObjectItem(json, "tags");
  len = cJSON_GetArraySize(arr);
  state.tags_data = g_ptr_array_new();
  for (int i = 0; i < len; i++) {
    item = cJSON_GetArrayItem(arr, i);
    g_ptr_array_add(state.tags_data, strdup(item->valuestring));
  }
  cJSON_Delete(json);
}

void errands_data_write() {
  LOG("Writing user data");

  cJSON *json = cJSON_CreateObject();

  // Write lists data
  cJSON *lists = cJSON_CreateArray();
  for (int i = 0; i < state.tl_data->len; i++) {
    TaskListData *data = state.tl_data->pdata[i];
    cJSON *l_data = cJSON_CreateObject();
    cJSON_AddItemToObject(l_data, "color", cJSON_CreateString(data->color));
    cJSON_AddItemToObject(l_data, "deleted", cJSON_CreateBool(data->deleted));
    cJSON_AddItemToObject(l_data, "name", cJSON_CreateString(data->name));
    cJSON_AddItemToObject(l_data, "show_completed", cJSON_CreateBool(data->show_completed));
    cJSON_AddItemToObject(l_data, "synced", cJSON_CreateBool(data->synced));
    cJSON_AddItemToObject(l_data, "uid", cJSON_CreateString(data->uid));
    cJSON_AddItemToArray(lists, l_data);
  }
  cJSON_AddItemToObject(json, "lists", lists);

  // Write tags data
  cJSON *tags = cJSON_CreateArray();
  for (int i = 0; i < state.tags_data->len; i++) {
    char *data = state.tags_data->pdata[i];
    cJSON_AddItemToArray(tags, cJSON_CreateString(data));
  }
  cJSON_AddItemToObject(json, "tags", tags);

  // Write tasks data
  cJSON *tasks = cJSON_CreateArray();
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *data = state.t_data->pdata[i];
    cJSON *t_data = cJSON_CreateObject();
    cJSON *attachments_array = cJSON_CreateArray();
    for (int j = 0; j < data->attachments->len; j++) {
      cJSON *attachment_item = cJSON_CreateString(data->attachments->pdata[j]);
      cJSON_AddItemToArray(attachments_array, attachment_item);
    }
    cJSON_AddItemToObject(t_data, "attachments", attachments_array);
    cJSON_AddItemToObject(t_data, "color", cJSON_CreateString(data->color));
    cJSON_AddItemToObject(t_data, "completed", cJSON_CreateBool(data->completed));
    cJSON_AddItemToObject(t_data, "changed_at", cJSON_CreateString(data->changed_at));
    cJSON_AddItemToObject(t_data, "created_at", cJSON_CreateString(data->created_at));
    cJSON_AddItemToObject(t_data, "deleted", cJSON_CreateBool(data->deleted));
    cJSON_AddItemToObject(t_data, "due_date", cJSON_CreateString(data->due_date));
    cJSON_AddItemToObject(t_data, "expanded", cJSON_CreateBool(data->expanded));
    cJSON_AddItemToObject(t_data, "list_uid", cJSON_CreateString(data->list_uid));
    cJSON_AddItemToObject(t_data, "notes", cJSON_CreateString(data->notes));
    cJSON_AddItemToObject(t_data, "notified", cJSON_CreateBool(data->notified));
    cJSON_AddItemToObject(t_data, "parent", cJSON_CreateString(data->parent));
    cJSON_AddItemToObject(t_data, "percent_complete", cJSON_CreateNumber(data->percent_complete));
    cJSON_AddItemToObject(t_data, "priority", cJSON_CreateNumber(data->priority));
    cJSON_AddItemToObject(t_data, "rrule", cJSON_CreateString(data->rrule));
    cJSON_AddItemToObject(t_data, "start_date", cJSON_CreateString(data->start_date));
    cJSON_AddItemToObject(t_data, "synced", cJSON_CreateBool(data->synced));
    cJSON *tags_array = cJSON_CreateArray();
    for (int j = 0; j < data->tags->len; j++) {
      cJSON *tag_item = cJSON_CreateString(data->tags->pdata[j]);
      cJSON_AddItemToArray(tags_array, tag_item);
    }
    cJSON_AddItemToObject(t_data, "tags", tags_array);
    cJSON_AddItemToObject(t_data, "text", cJSON_CreateString(data->text));
    cJSON_AddItemToObject(t_data, "toolbar_shown", cJSON_CreateBool(data->toolbar_shown));
    cJSON_AddItemToObject(t_data, "trash", cJSON_CreateBool(data->trash));
    cJSON_AddItemToObject(t_data, "uid", cJSON_CreateString(data->uid));

    cJSON_AddItemToArray(tasks, t_data);
  }
  cJSON_AddItemToObject(json, "tasks", tasks);

  // Save to file
  char *json_string = cJSON_PrintUnformatted(json);
  char *data_file_path = g_build_path("/", g_get_user_data_dir(), "errands", "data.json", NULL);

  write_string_to_file(data_file_path, json_string);

  // Clean up
  cJSON_Delete(json);
  free(json_string);
  g_free(data_file_path);
}

// --- LISTS --- //

TaskListData *errands_data_add_list(const char *name) {
  TaskListData *tl = malloc(sizeof(*tl));
  generate_hex(tl->color);
  tl->deleted = false;
  strncpy(tl->name, name, 255);
  tl->show_completed = true;
  tl->synced = false;
  generate_uuid(tl->uid);
  g_ptr_array_add(state.tl_data, tl);
  errands_data_write();
  return tl;
}

void errands_data_delete_list(TaskListData *data) {
  LOG("Data: Deleting task list '%s'", data->uid);
  // Mark list as deleted
  data->deleted = true;
  // Remove tasks
  GPtrArray *new_t_data = g_ptr_array_new();
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    LOG("Data: deleting task '%s'", td->uid);
    if (!strcmp(data->uid, td->list_uid))
      errands_data_free_task(td);
    else
      g_ptr_array_add(new_t_data, td);
  }
  g_ptr_array_free(state.t_data, TRUE);
  state.t_data = new_t_data;
  return;
}

char *errands_data_task_list_as_ical(TaskListData *data) {
  str ical = str_new("BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:-//Errands\n");
  str_append_printf(&ical, "X-WR-CALNAME:%s\n", data->name);
  str_append_printf(&ical, "X-APPLE-CALENDAR-COLOR:%s\n", data->color);
  // Add tasks
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    char *task_ical = errands_data_task_as_ical(td);
    str_append(&ical, task_ical);
    free(task_ical);
  }
  str_append(&ical, "END:VCALENDAR\n");
  return ical.str;
}

TaskListData *errands_task_list_from_ical(const char *ical) {
  TaskListData *data = malloc(sizeof(*data));
  char *name = get_ical_value(ical, "X-WR-CALNAME");
  name ? strncpy(data->name, name, 255) : strcpy(data->name, "New List");
  free(name);
  char *color = get_ical_value(ical, "X-APPLE-CALENDAR-COLOR");
  color ? strcpy(data->color, color) : generate_hex(data->color);
  free(color);
  data->deleted = false;
  data->show_completed = true;
  data->synced = false;
  generate_uuid(data->uid);
  return data;
}

// --- TASKS --- //

TaskData *errands_data_add_task(char *text, char *list_uid, char *parent_uid) {
  TaskData *t = malloc(sizeof(*t));
  t->attachments = g_ptr_array_new();
  strcpy(t->color, "");
  t->completed = false;
  get_date_time(t->changed_at);
  get_date_time(t->created_at);
  t->deleted = false;
  strcpy(t->due_date, "");
  t->expanded = false;
  strcpy(t->list_uid, list_uid);
  t->notes = strdup("");
  t->notified = false;
  strcpy(t->parent, parent_uid);
  t->percent_complete = 0;
  t->priority = 0;
  t->rrule = strdup("");
  strcpy(t->start_date, "");
  t->synced = false;
  t->tags = g_ptr_array_new();
  t->text = strdup(text);
  t->toolbar_shown = false;
  t->trash = false;
  generate_uuid(t->uid);
  g_ptr_array_insert(state.t_data, 0, t);
  errands_data_write();
  return t;
}

void errands_data_free_task(TaskData *data) {
  g_ptr_array_free(data->attachments, true);
  free(data->notes);
  free(data->rrule);
  g_ptr_array_free(data->tags, true);
  free(data->text);
}

TaskData *errands_data_get_task(char *uid) {
  TaskData *td = NULL;
  for (int i = 0; i < state.t_data->len; i++) {
    td = state.t_data->pdata[i];
    if (!strcmp(td->uid, uid))
      break;
  }
  return td;
}

char *errands_data_task_as_ical(TaskData *data) {
  str ical = str_new("BEGIN:VTODO\n");
  // Add UID and SUMMARY
  str_append_printf(&ical, "UID:%s\nSUMMARY:%s\n", data->uid, data->text ? data->text : "");
  if (strcmp(data->color, ""))
    str_append_printf(&ical, "X-ERRANDS-COLOR:%s\n", data->color);
  // Add start date if available
  if (strcmp(data->start_date, ""))
    str_append_printf(&ical, "DTSTART:%s\n", data->start_date);
  // Add due date if available
  if (strcmp(data->due_date, ""))
    str_append_printf(&ical, "DUE:%s\n", data->due_date);
  // Add recurrence rule if available
  if (strcmp(data->rrule, ""))
    str_append_printf(&ical, "RRULE:%s\n", data->rrule);
  // Add description (notes) if available
  if (strcmp(data->notes, ""))
    str_append_printf(&ical, "DESCRIPTION:%s\n", data->notes);
  // Add priority
  str_append_printf(&ical, "PRIORITY:%d\n", data->priority);
  // Add percent complete
  str_append_printf(&ical, "PERCENT-COMPLETE:%d\n", data->percent_complete);
  // Add completion status
  if (data->completed)
    str_append(&ical, "STATUS:COMPLETED\n");
  else
    str_append(&ical, "STATUS:IN-PROCESS\n");
  // Add changed_at if available
  if (strcmp(data->changed_at, ""))
    str_append_printf(&ical, "LAST-MODIFIED:%s\n", data->changed_at);
  // Add created_at if available
  if (strcmp(data->created_at, ""))
    str_append_printf(&ical, "CREATED:%s\n", data->created_at);
  // Add parent if available
  if (strcmp(data->parent, ""))
    str_append_printf(&ical, "RELATED-TO:%s\n", data->parent);
  // Add tags if available
  if (data->tags->len > 0) {
    str_append(&ical, "CATEGORIES:");
    for (guint i = 0; i < data->tags->len; i++) {
      char *tag = data->tags->pdata[i];
      if (tag && strcmp(tag, ""))
        str_append(&ical, tag);
    }
    str_append(&ical, "\n");
  }
  // End the VTODO and VCALENDAR sections
  str_append(&ical, "END:VTODO\n");
  return ical.str;
}

GPtrArray *errands_data_tasks_from_ical(const char *ical, const char *list_uid) {
  GPtrArray *out = g_ptr_array_new();
  GPtrArray *todos = get_vtodos(ical);
  if (todos) {
    for_range(i, 0, todos->len) {
      const char *todo = todos->pdata[i];
      TaskData *t = malloc(sizeof(*t));

      t->attachments = g_ptr_array_new();

      char *color = get_ical_value(todo, "X-ERRANDS-COLOR");
      color ? strcpy(t->color, color) : strcpy(t->color, "");
      free(color);

      char *completed = get_ical_value(todo, "STATUS");
      if (completed) {
        t->completed = !strcmp(completed, "COMPLETED") ? true : false;
        free(completed);
      }

      char *changed_at = get_ical_value(todo, "LAST-MODIFIED");
      changed_at ? strcpy(t->changed_at, changed_at) : get_date_time(t->changed_at);
      free(changed_at);

      char *created_at = get_ical_value(todo, "CREATED");
      created_at ? strcpy(t->created_at, created_at) : get_date_time(t->created_at);
      free(created_at);

      t->deleted = false;

      char *due = get_ical_value(todo, "DUE");
      due ? strcpy(t->due_date, due) : strcpy(t->due_date, "");
      free(due);

      t->expanded = false;
      strcpy(t->list_uid, list_uid);

      char *notes = get_ical_value(todo, "DESCRIPTION");
      t->notes = notes ? notes : strdup("");

      t->notified = false;

      char *parent = get_ical_value(todo, "RELATED-TO");
      parent ? strcpy(t->parent, parent) : strcpy(t->parent, "");

      char *percent_complete = get_ical_value(todo, "PERCENT-COMPLETE");
      t->percent_complete = percent_complete ? atoi(percent_complete) : 0;
      if (percent_complete)
        free(percent_complete);

      char *priority = get_ical_value(todo, "PRIORITY");
      t->priority = priority ? atoi(priority) : 0;
      if (priority)
        free(priority);

      char *rrule = get_ical_value(todo, "RRULE");
      t->rrule = rrule ? rrule : strdup("");

      char *start_date = get_ical_value(todo, "DTSTART");
      start_date ? strcpy(t->start_date, start_date) : strcpy(t->start_date, "");
      free(start_date);

      t->synced = false;
      t->tags = g_ptr_array_new();

      char *text = get_ical_value(todo, "SUMMARY");
      t->text = text ? text : strdup("Task Text");

      t->toolbar_shown = false;
      t->trash = false;
      generate_uuid(t->uid);

      g_ptr_array_add(out, t);
    }
    g_ptr_array_free(todos, true);
  }
  return out;
}

// --- PRINTING --- //

#define MAX_LINE_LENGTH 73

static void __print_indent(GString *out, int indent) {
  for (int i = 0; i < indent; i++)
    g_string_append(out, "    ");
}

static void errands_print_task(TaskData *task, GString *out, int indent) {
  __print_indent(out, indent);
  // Add checkbox
  g_string_append_printf(out, "[%s] ", task->completed ? "x" : " ");
  // Set idx to account for indentation and checkbox
  int idx = indent * 4; // The checkbox takes 4 characters
  for (int i = 0; i < strlen(task->text); i++) {
    // If idx exceeds MAX_LINE_LENGTH, start a new line with indentation
    if (idx >= MAX_LINE_LENGTH) {
      g_string_append(out, "\n");
      if (indent == 0)
        __print_indent(out, 1);
      else
        __print_indent(out, indent + 1);
      idx = indent * 4;
    }
    g_string_append_printf(out, "%c", task->text[i]);
    idx++;
  }
  g_string_append(out, "\n");
}

static void errands_print_tasks(GString *out, const char *parent_uid, const char *list_uid,
                                int indent) {
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!strcmp(td->parent, parent_uid) && !strcmp(td->list_uid, list_uid)) {
      errands_print_task(td, out, indent);
      errands_print_tasks(out, td->uid, list_uid, indent + 1);
    }
  }
}

GString *errands_data_print_list(char *list_uid) {
  // Output string
  GString *out = g_string_new("");

  // Print list name
  char *list_name;
  for (int i = 0; i < state.tl_data->len; i++) {
    TaskListData *tld = state.tl_data->pdata[i];
    if (!strcmp(list_uid, tld->uid)) {
      list_name = strdup(tld->name);
      break;
    }
  }
  g_string_append(out, "╔");
  for_range(i, 0, MAX_LINE_LENGTH + 2) g_string_append(out, "═");
  g_string_append(out, "╗\n");
  g_string_append(out, "║ ");

  int len = strlen(list_name);
  // If name is too long add '...' to the end
  if (len > MAX_LINE_LENGTH - 1) {
    list_name[MAX_LINE_LENGTH - 1] = '.';
    list_name[MAX_LINE_LENGTH - 2] = '.';
    list_name[MAX_LINE_LENGTH - 3] = '.';
    for_range(i, 0, MAX_LINE_LENGTH) { g_string_append_printf(out, "%c", list_name[i]); }
  } else if (len < MAX_LINE_LENGTH) {
    GString *title = g_string_new(list_name);
    int title_len = title->len;
    while (title_len <= MAX_LINE_LENGTH - 1) {
      g_string_prepend(title, " ");
      title_len++;
      if (title_len < MAX_LINE_LENGTH - 1) {
        g_string_append(title, " ");
        title_len++;
      }
    }
    g_string_append(out, title->str);
    g_string_free(title, TRUE);
  }
  free(list_name);
  g_string_append(out, " ║\n");
  g_string_append(out, "╚");
  for_range(i, 0, MAX_LINE_LENGTH + 2) g_string_append(out, "═");
  g_string_append(out, "╝\n");
  // Print tasks
  errands_print_tasks(out, "", list_uid, 0);
  return out;
}

// --- TAGS --- //

void errands_data_tag_add(char *tag) {
  // Return if empty
  if (!strcmp(tag, ""))
    return;
  // Return if has duplicates
  for (int i = 0; i < state.tags_data->len; i++)
    if (!strcmp(tag, state.tags_data->pdata[i]))
      return;
  g_ptr_array_insert(state.tags_data, 0, strdup(tag));
}

void errands_data_tag_remove(char *tag) {
  // Remove from tags
  for (int i = 0; i < state.tags_data->len; i++)
    if (!strcmp(tag, state.tags_data->pdata[i])) {
      char *_tag = g_ptr_array_steal_index(state.tags_data, i);
      free(_tag);
    }
  // Remove from tasks's tags
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    for (int j = 0; j < td->tags->len; j++)
      if (!strcmp(tag, td->tags->pdata[j])) {
        char *_tag = g_ptr_array_steal_index(td->tags, j);
        free(_tag);
        j--;
      }
  }
}

void errands_data_tags_free() { g_ptr_array_free(state.tags_data, true); }
