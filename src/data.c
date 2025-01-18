#include "data.h"

#include "lib/cJSON.h"
#include "utils.h"

#include <glib.h>
#include <libical/ical.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_SEP "/"
const char *user_dir;

// --- ICAL UTILS --- //

// Get X property
icalproperty *__get_x_prop(icalcomponent *ical, const char *xprop, const char *default_val) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_X_PROPERTY);
  while (property) {
    const char *name = icalproperty_get_x_name(property);
    if (name && !strcmp(name, xprop))
      return property;
    property = icalcomponent_get_next_property(ical, ICAL_X_PROPERTY);
  }
  // Create if not exists
  property = icalproperty_new_x(default_val);
  icalproperty_set_x_name(property, xprop);
  icalcomponent_add_property(ical, property);
  return property;
}

// Get X property value string
const char *__get_x_prop_value(icalcomponent *ical, const char *xprop, const char *default_val) {
  icalproperty *property = __get_x_prop(ical, xprop, default_val);
  return icalproperty_get_value_as_string(property);
}

void __set_x_prop_value(icalcomponent *ical, const char *xprop, const char *val) {
  icalproperty *property = __get_x_prop(ical, xprop, val);
  icalproperty_set_x(property, val);
}

const char *__get_prop_value_str(icalcomponent *ical, icalproperty_kind kind,
                                 const char *default_val) {
  icalproperty *property = icalcomponent_get_first_property(ical, kind);
  if (property)
    return icalproperty_get_value_as_string(property);
  if (default_val) {
    property = icalproperty_new(kind);
    icalproperty_set_value(property, icalvalue_new_from_string(ICAL_STRING_VALUE, default_val));
    icalcomponent_add_property(ical, property);
    return default_val;
  }
  return NULL;
}

const char *__get_prop_value_or(icalcomponent *ical, icalproperty_kind kind, const char *or_prop) {
  icalproperty *property = icalcomponent_get_first_property(ical, kind);
  if (property)
    return icalproperty_get_value_as_string(property);
  return or_prop;
}

void __set_prop_str(icalcomponent *ical, icalproperty_kind kind, const char *val) {
  icalproperty *property = icalcomponent_get_first_property(ical, kind);
  if (property)
    icalproperty_set_value(property, icalvalue_new_from_string(ICAL_ANY_VALUE, val));
  else {
    property = icalproperty_new(kind);
    icalproperty_set_value(property, icalvalue_new_from_string(ICAL_ANY_VALUE, val));
    icalcomponent_add_property(ical, property);
  }
}
void __set_prop_int(icalcomponent *ical, icalproperty_kind kind, int val) {
  g_autofree gchar *str = g_strdup_printf("%d", val);
  __set_prop_str(ical, kind, str);
}
void __set_prop_bool(icalcomponent *ical, icalproperty_kind kind, bool val) {
  __set_prop_str(ical, kind, val ? "1" : "0");
}

// --- LIST DATA --- //

ListData *list_data_new(const char *uid) {
  icalcomponent *cal = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
  __set_x_prop_value(cal, "X-ERRANDS-LIST-UID", uid);
  return cal;
}

void list_data_free(ListData *data) { icalcomponent_free(data); }

GPtrArray *list_data_get_tasks(ListData *data) {
  GPtrArray *tasks = g_ptr_array_new();
  icalcomponent *c;
  for (c = icalcomponent_get_first_component(data, ICAL_VTODO_COMPONENT); c != 0;
       c = icalcomponent_get_next_component(data, ICAL_VTODO_COMPONENT))
    g_ptr_array_add(tasks, c);
  return tasks;
}

TaskData *list_data_create_task(ListData *list, const char *text, const char *list_uid,
                                const char *parent) {
  TaskData *task_data = icalcomponent_new(ICAL_VTODO_COMPONENT);
  icalcomponent_add_component(list, task_data);
  return task_data;
}

const char *list_data_get_color(ListData *data) {
  return __get_x_prop_value(data, "X-ERRANDS-COLOR", "");
}
void list_data_set_color(ListData *data, const char *color) {
  __set_x_prop_value(data, "X-ERRANDS-COLOR", color);
}
const char *list_data_get_name(ListData *data) {
  return __get_x_prop_value(data, "X-WR-CALNAME", "Untitled");
}
void list_data_set_name(ListData *data, const char *name) {
  __set_x_prop_value(data, "X-WR-CALNAME", name);
}
const char *list_data_get_uid(ListData *data) {
  return __get_x_prop_value(data, "X-ERRANDS-LIST-UID", "");
}
void list_data_set_uid(ListData *data, const char *uid) {
  __set_x_prop_value(data, "X-ERRANDS-LIST-UID", uid);
}
bool list_data_get_deleted(ListData *data) {
  return (bool)atoi(__get_x_prop_value(data, "X-ERRANDS-DELETED", "0"));
}
void list_data_set_deleted(ListData *data, bool deleted) {
  __set_x_prop_value(data, "X-ERRANDS-DELETED", deleted ? "1" : "0");
}
bool list_data_get_synced(ListData *data) {
  return (bool)atoi(__get_x_prop_value(data, "X-ERRANDS-SYNCED", "0"));
}
void list_data_set_synced(ListData *data, bool synced) {
  __set_x_prop_value(data, "X-ERRANDS-SYNCED", synced ? "1" : "0");
}

// --- TASK DATA --- //

TaskData *task_data_new(ListData *list, const char *text, const char *list_uid) {
  icalcomponent *task = icalcomponent_new(ICAL_VTODO_COMPONENT);
  return task;
}
void task_data_free(TaskData *data) { free(data); }
ListData *task_data_get_list(TaskData *data) { return icalcomponent_get_parent(data); }

const char *task_data_get_changed(ListData *data) {
  return __get_prop_value_or(data, ICAL_LASTMODIFIED_PROPERTY, "");
}
void task_data_set_changed(ListData *data, const char *changed) {
  __set_prop_str(data, ICAL_LASTMODIFIED_PROPERTY, changed);
}
const char *task_data_get_created(ListData *data) {
  return __get_prop_value_or(data, ICAL_DTSTAMP_PROPERTY, "");
}
void task_data_set_created(ListData *data, const char *created) {
  __set_prop_str(data, ICAL_DTSTAMP_PROPERTY, created);
}
const char *task_data_get_due(ListData *data) {
  return __get_prop_value_or(data, ICAL_DUE_PROPERTY, "");
}
void task_data_set_due(ListData *data, const char *due) {
  __set_prop_str(data, ICAL_DUE_PROPERTY, due);
}
const char *task_data_get_notes(ListData *data) {
  return __get_prop_value_or(data, ICAL_DESCRIPTION_PROPERTY, "");
}
void task_data_set_notes(ListData *data, const char *notes) {
  __set_prop_str(data, ICAL_DESCRIPTION_PROPERTY, notes);
}
const char *task_data_get_parent(ListData *data) {
  return __get_prop_value_or(data, ICAL_RELATEDTO_PROPERTY, "");
}
void task_data_set_parent(ListData *data, const char *parent) {
  __set_prop_str(data, ICAL_RELATEDTO_PROPERTY, parent);
}
const char *task_data_get_start(ListData *data) {
  return __get_prop_value_or(data, ICAL_DTSTART_PROPERTY, "");
}
void task_data_set_start(ListData *data, const char *start) {
  __set_prop_str(data, ICAL_DTSTART_PROPERTY, start);
}
const char *task_data_get_rrule(ListData *data) {
  return __get_prop_value_or(data, ICAL_RRULE_PROPERTY, "");
}
void task_data_set_rrule(ListData *data, const char *rrule) {
  __set_prop_str(data, ICAL_RRULE_PROPERTY, rrule);
}
const char *task_data_get_text(ListData *data) {
  return __get_prop_value_or(data, ICAL_SUMMARY_PROPERTY, "");
}
void task_data_set_text(ListData *data, const char *text) {
  __set_prop_str(data, ICAL_SUMMARY_PROPERTY, text);
}
const char *task_data_get_uid(ListData *data) {
  return __get_prop_value_or(data, ICAL_UID_PROPERTY, "");
}
void task_data_set_uid(ListData *data, const char *uid) {
  __set_prop_str(data, ICAL_UID_PROPERTY, uid);
}
bool task_data_get_completed(ListData *data) {
  return __get_prop_value_or(data, ICAL_COMPLETED_PROPERTY, NULL) ? true : false;
}
void task_data_set_completed(ListData *data, bool completed) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_COMPLETED_PROPERTY);
  if (completed) {
    char dt[17];
    get_date_time(dt);
    if (prop)
      icalproperty_set_completed(prop, icaltime_from_string(dt));
    else
      prop = icalproperty_new_completed(icaltime_from_string(dt));
  } else {
    if (prop)
      icalcomponent_remove_property(data, prop);
  }
}
const char *task_data_get_tags(ListData *data) {
  return __get_prop_value_or(data, ICAL_CATEGORIES_PROPERTY, "");
}
void task_data_set_tags(ListData *data, const char *tags) {
  __set_prop_str(data, ICAL_CATEGORIES_PROPERTY, tags);
}
uint8_t task_data_get_percent(ListData *data) {
  return atoi(__get_prop_value_or(data, ICAL_PERCENTCOMPLETE_PROPERTY, ""));
}
void task_data_set_percent(ListData *data, uint8_t percent) {
  __set_prop_int(data, ICAL_PERCENTCOMPLETE_PROPERTY, percent);
}
uint8_t task_data_get_priority(ListData *data) {
  return atoi(__get_prop_value_or(data, ICAL_PRIORITY_PROPERTY, ""));
}
void task_data_set_priority(ListData *data, uint8_t priority) {
  __set_prop_int(data, ICAL_PRIORITY_PROPERTY, priority);
}
const char *task_data_get_attachments(ListData *data) {
  return __get_x_prop_value(data, "X-ERRANDS-ATTACHMENTS", "");
}
void task_data_set_attachments(ListData *data, const char *attachments) {
  __set_x_prop_value(data, "X-ERRANDS-ATTACHMENTS", attachments);
}
const char *task_data_get_color(ListData *data) {
  return __get_x_prop_value(data, "X-ERRANDS-COLOR", "");
}
void task_data_set_color(ListData *data, const char *color) {
  __set_x_prop_value(data, "X-ERRANDS-COLOR", color);
}
const char *task_data_get_list_uid(ListData *data) {
  return __get_x_prop_value(data, "X-ERRANDS-LIST-UID", "");
}
void task_data_set_list_uid(ListData *data, const char *list_uid) {
  __set_x_prop_value(data, "X-ERRANDS-LIST-UID", list_uid);
}
bool task_data_get_deleted(ListData *data) {
  return (bool)atoi(__get_x_prop_value(data, "X-ERRANDS-DELETED", "0"));
}
void task_data_set_deleted(ListData *data, bool deleted) {
  __set_x_prop_value(data, "X-ERRANDS-DELETED", deleted ? "1" : "0");
}
bool task_data_get_expanded(ListData *data) {
  return (bool)atoi(__get_x_prop_value(data, "X-ERRANDS-EXPANDED", "0"));
}
void task_data_set_expanded(ListData *data, bool expanded) {
  __set_x_prop_value(data, "X-ERRANDS-EXPANDED", expanded ? "1" : "0");
}
bool task_data_get_notified(ListData *data) {
  return (bool)atoi(__get_x_prop_value(data, "X-ERRANDS-NOTIFIED", "0"));
}
void task_data_set_notified(ListData *data, bool notified) {
  __set_x_prop_value(data, "X-ERRANDS-NOTIFIED", notified ? "1" : "0");
}
bool task_data_get_synced(ListData *data) {
  return (bool)atoi(__get_x_prop_value(data, "X-ERRANDS-SYNCED", "0"));
}
void task_data_set_synced(ListData *data, bool synced) {
  __set_x_prop_value(data, "X-ERRANDS-SYNCED", synced ? "1" : "0");
}
bool task_data_get_toolbar_shown(ListData *data) {
  return (bool)atoi(__get_x_prop_value(data, "X-ERRANDS-TOOLBAR-SHOWN", "0"));
}
void task_data_set_toolbar_shown(ListData *data, bool toolbar_shown) {
  __set_x_prop_value(data, "X-ERRANDS-TOOLBAR-SHOWN", toolbar_shown ? "1" : "0");
}
bool task_data_get_trash(ListData *data) {
  return (bool)atoi(__get_x_prop_value(data, "X-ERRANDS-TRASH", "0"));
}
void task_data_set_trash(ListData *data, bool trash) {
  __set_x_prop_value(data, "X-ERRANDS-TRASH", trash ? "1" : "0");
}

// --- LOADING --- //

static void errands_data_migrate() {
  g_autofree gchar *old_data_file = g_build_path(PATH_SEP, user_dir, "data.json", NULL);
  if (!file_exists(old_data_file))
    return;
  LOG("User Data: Migrate");
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
    __set_x_prop_value(calendar, "X-WR-CALNAME", name);
    __set_x_prop_value(calendar, "X-ERRANDS-COLOR", color);
    __set_x_prop_value(calendar, "X-ERRANDS-DELETED", deleted ? "1" : "0");
    __set_x_prop_value(calendar, "X-ERRANDS-SYNCED", synced ? "1" : "0");
    __set_x_prop_value(calendar, "X-ERRANDS-LIST-UID", list_uid);
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
      __set_x_prop_value(event, "X-ERRANDS-ATTACHMENTS", attachments->str);
      __set_x_prop_value(event, "X-ERRANDS-COLOR", !strcmp(color, "") ? "none" : color);
      __set_x_prop_value(event, "X-ERRANDS-DELETED", deleted ? "1" : "0");
      __set_x_prop_value(event, "X-ERRANDS-EXPANDED", expanded ? "1" : "0");
      __set_x_prop_value(event, "X-ERRANDS-NOTIFIED", notified ? "1" : "0");
      __set_x_prop_value(event, "X-ERRANDS-SYNCED", synced ? "1" : "0");
      __set_x_prop_value(event, "X-ERRANDS-TOOLBAR-SHOWN", toolbar_shown ? "1" : "0");
      __set_x_prop_value(event, "X-ERRANDS-TRASH", trash ? "1" : "0");
      __set_x_prop_value(event, "X-ERRANDS-LIST-UID", task_list_uid);

      icalcomponent_add_component(calendar, event);
    }
    g_autofree gchar *calendar_filename = g_strdup_printf("%s.ics", list_uid);
    g_autofree gchar *calendar_file_path =
        g_build_path(PATH_SEP, user_dir, calendar_filename, NULL);
    write_string_to_file(calendar_file_path, icalcomponent_as_ical_string(calendar));
  }
  cJSON_Delete(json);
  remove(old_data_file);
}

GPtrArray *errands_data_load_lists() {
  user_dir = g_build_path(PATH_SEP, g_get_user_data_dir(), "errands", NULL);
  if (!directory_exists(user_dir)) {
    LOG("User Data: Creating user data directory at %s", user_dir);
    g_mkdir_with_parents(user_dir, 0755);
  }
  LOG("User Data: Loading at %s", user_dir);
  errands_data_migrate();
  GPtrArray *array = g_ptr_array_new();
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
        LOG("User Data: Loading calendar %s", path);
        icalcomponent *calendar = icalparser_parse_string(content);
        if (calendar) {
          *(strstr(filename, ".")) = '\0';
          g_ptr_array_add(array, calendar);
        }
        free(content);
      }
    }
  }
  return array;
}

GPtrArray *errands_data_load_tasks(GPtrArray *calendars) {
  GPtrArray *events = g_ptr_array_new();
  for (size_t i = 0; i < calendars->len; i++) {
    ListData *calendar = calendars->pdata[i];
    icalcomponent *c;
    for (c = icalcomponent_get_first_component(calendar, ICAL_VTODO_COMPONENT); c != 0;
         c = icalcomponent_get_next_component(calendar, ICAL_VTODO_COMPONENT))
      g_ptr_array_add(events, c);
  }
  return events;
}

void errands_data_write_list(ListData *data) {
  g_autofree gchar *filename = g_strdup_printf("%s.ics", list_data_get_uid(data));
  g_autofree gchar *path = g_build_path(PATH_SEP, user_dir, filename, NULL);
  const char *ical_string = icalcomponent_as_ical_string(data);
  write_string_to_file(path, ical_string);
  LOG("User Data: Save list %s", path);
}

void errands_data_write_lists(GPtrArray *lists) {
  for (size_t i = 0; i < lists->len; i++)
    errands_data_write_list(lists->pdata[i]);
}

// --- PRINTING --- //

// #define MAX_LINE_LENGTH 73

// static void __print_indent(GString *out, int indent) {
//   for (int i = 0; i < indent; i++)
//     g_string_append(out, "    ");
// }

// static void errands_print_task(TaskData *task, GString *out, int indent) {
//   __print_indent(out, indent);
//   // Add checkbox
//   g_string_append_printf(out, "[%s] ", task->completed ? "x" : " ");
//   // Set idx to account for indentation and checkbox
//   int idx = indent * 4; // The checkbox takes 4 characters
//   for (int i = 0; i < strlen(task->text); i++) {
//     // If idx exceeds MAX_LINE_LENGTH, start a new line with indentation
//     if (idx >= MAX_LINE_LENGTH) {
//       g_string_append(out, "\n");
//       if (indent == 0)
//         __print_indent(out, 1);
//       else
//         __print_indent(out, indent + 1);
//       idx = indent * 4;
//     }
//     g_string_append_printf(out, "%c", task->text[i]);
//     idx++;
//   }
//   g_string_append(out, "\n");
// }

// static void errands_print_tasks(GString *out, const char *parent_uid, const char *list_uid,
//                                 int indent) {
//   for (int i = 0; i < state.t_data->len; i++) {
//     TaskData *td = state.t_data->pdata[i];
//     if (!strcmp(td->parent, parent_uid) && !strcmp(td->list_uid, list_uid)) {
//       errands_print_task(td, out, indent);
//       errands_print_tasks(out, td->uid, list_uid, indent + 1);
//     }
//   }
// }

// GString *errands_data_print_list(char *list_uid) {
//   // Output string
//   GString *out = g_string_new("");

//   // Print list name
//   char *list_name;
//   for (int i = 0; i < state.tl_data->len; i++) {
//     TaskListData *tld = state.tl_data->pdata[i];
//     if (!strcmp(list_uid, tld->uid)) {
//       list_name = strdup(tld->name);
//       break;
//     }
//   }
//   g_string_append(out, "╔");
//   for_range(i, 0, MAX_LINE_LENGTH + 2) g_string_append(out, "═");
//   g_string_append(out, "╗\n");
//   g_string_append(out, "║ ");

//   int len = strlen(list_name);
//   // If name is too long add '...' to the end
//   if (len > MAX_LINE_LENGTH - 1) {
//     list_name[MAX_LINE_LENGTH - 1] = '.';
//     list_name[MAX_LINE_LENGTH - 2] = '.';
//     list_name[MAX_LINE_LENGTH - 3] = '.';
//     for_range(i, 0, MAX_LINE_LENGTH) { g_string_append_printf(out, "%c", list_name[i]); }
//   } else if (len < MAX_LINE_LENGTH) {
//     GString *title = g_string_new(list_name);
//     int title_len = title->len;
//     while (title_len <= MAX_LINE_LENGTH - 1) {
//       g_string_prepend(title, " ");
//       title_len++;
//       if (title_len < MAX_LINE_LENGTH - 1) {
//         g_string_append(title, " ");
//         title_len++;
//       }
//     }
//     g_string_append(out, title->str);
//     g_string_free(title, TRUE);
//   }
//   free(list_name);
//   g_string_append(out, " ║\n");
//   g_string_append(out, "╚");
//   for_range(i, 0, MAX_LINE_LENGTH + 2) g_string_append(out, "═");
//   g_string_append(out, "╝\n");
//   // Print tasks
//   errands_print_tasks(out, "", list_uid, 0);
//   return out;
// }
