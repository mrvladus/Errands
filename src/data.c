#include "data.h"
#include "lib/cJSON.h"
#include "settings.h"
#include "utils.h"

#define PATH_SEP "/"
const char *user_dir;

GPtrArray *ldata = NULL;
GPtrArray *tdata = NULL;

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

// --- LIST DATA --- //

ListData *list_data_new(const char *uid, const char *name, const char *color, bool deleted,
                        bool synced, int position) {
  icalcomponent *cal = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
  icalcomponent_add_property(cal, icalproperty_new_version("2.0"));
  icalcomponent_add_property(cal, icalproperty_new_prodid("~//Errands"));
  if (!uid) {
    g_autofree gchar *_uid = g_uuid_string_random();
    __set_x_prop_value(cal, "X-ERRANDS-LIST-UID", _uid);
  } else {
    __set_x_prop_value(cal, "X-ERRANDS-LIST-UID", uid);
  }
  if (!color || !strcmp(color, "")) {
    char _color[8];
    generate_hex(_color);
    __set_x_prop_value(cal, "X-ERRANDS-COLOR", _color);
  } else {
    __set_x_prop_value(cal, "X-ERRANDS-COLOR", color);
  }
  list_data_set_synced(cal, synced);
  list_data_set_deleted(cal, deleted);
  list_data_set_position(cal, position);
  list_data_set_name(cal, name);
  return cal;
}

ListData *list_data_new_from_ical(const char *ical, const char *uid, size_t position) {
  ListData *data = icalcomponent_new_from_string(ical);
  if (!data)
    return NULL;

  __get_x_prop_value(data, "X-ERRANDS-LIST-UID", uid);
  g_autofree gchar *pos = g_strdup_printf("%zu", position);
  __set_x_prop_value(data, "X-ERRANDS-POSITION", pos);
  __get_x_prop_value(data, "X-ERRANDS-DELETED", "0");
  char _color[8];
  generate_hex(_color);
  __get_x_prop_value(data, "X-ERRANDS-COLOR", _color);

  return data;
}

void list_data_update_positions() {
  for (size_t i = 0; i < ldata->len; i++)
    list_data_set_position(ldata->pdata[i], i);
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
  g_autofree gchar *uid = g_uuid_string_random();
  task_data_set_uid(task_data, uid);
  task_data_set_text(task_data, text);
  if (strcmp(parent, ""))
    task_data_set_parent(task_data, parent);
  task_data_set_list_uid(task_data, list_uid);
  icalcomponent_add_component(list, task_data);
  return task_data;
}

gchar *list_data_print(ListData *data) {
  const char *list_uid = list_data_get_uid(data);
  const char *name = list_data_get_name(data);
  size_t len = strlen(name);
  char *list_name = strndup(name, len > 72 ? 72 : len);
  // Print list name
  GString *out = g_string_new(list_name);
  g_string_append(out, "\n\n");
  // Print tasks
  GPtrArray *tasks = list_data_get_tasks(data);
  for (size_t i = 0; i < tasks->len; i++)
    task_data_print(tasks->pdata[i], out, 0);
  free(list_name);
  g_ptr_array_free(tasks, false);
  return g_string_free(out, false);
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
int list_data_get_position(ListData *data) {
  return atoi(__get_x_prop_value(data, "X-ERRANDS-POSITION", "0"));
}
void list_data_set_position(ListData *data, int position) {
  g_autofree gchar *pos = g_strdup_printf("%d", position);
  __set_x_prop_value(data, "X-ERRANDS-POSITION", pos);
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

GPtrArray *task_data_get_children(TaskData *data) {
  const char *uid = task_data_get_uid(data);
  ListData *list = task_data_get_list(data);
  GPtrArray *tasks = list_data_get_tasks(list);
  GPtrArray *children = g_ptr_array_new();
  for (size_t i = 0; i < tasks->len; i++)
    if (!strcmp(uid, task_data_get_parent(tasks->pdata[i])))
      g_ptr_array_add(children, tasks->pdata[i]);
  g_ptr_array_free(tasks, false);
  return children;
}

void task_data_print(TaskData *data, GString *out, size_t indent) {
  for (size_t i = 0; i < indent; i++)
    g_string_append(out, "  ");
  g_string_append_printf(out, "[%s] ", task_data_get_completed(data) ? "x" : " ");
  const char *text = task_data_get_text(data);
  size_t count = 0;
  char c = text[0];
  while (c != '\0') {
    g_string_append_c(out, c);
    count++;
    c = text[count];
    if (count % 72 == 0) {
      g_string_append_c(out, '\n');
      for (size_t i = 0; i < indent; i++)
        g_string_append(out, "  ");
      g_string_append(out, "    ");
    }
  }
  g_string_append_c(out, '\n');
  GPtrArray *children = task_data_get_children(data);
  indent++;
  for (size_t i = 0; i < children->len; i++)
    task_data_print(children->pdata[i], out, indent);
}

const char *task_data_get_changed(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_LASTMODIFIED_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_changed(TaskData *data, const char *changed) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_LASTMODIFIED_PROPERTY);
  if (prop) {
    icalproperty_set_lastmodified(prop, icaltime_from_string(changed));
  } else {
    prop = icalproperty_new_lastmodified(icaltime_from_string(changed));
    icalcomponent_add_property(data, prop);
  }
}
const char *task_data_get_created(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_DTSTAMP_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_created(TaskData *data, const char *created) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_DTSTAMP_PROPERTY);
  if (prop) {
    icalproperty_set_dtstamp(prop, icaltime_from_string(created));
  } else {
    prop = icalproperty_new_dtstamp(icaltime_from_string(created));
    icalcomponent_add_property(data, prop);
  }
}
const char *task_data_get_due(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_DUE_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_due(TaskData *data, const char *due) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_DUE_PROPERTY);
  if (prop) {
    icalproperty_set_due(prop, icaltime_from_string(due));
  } else {
    prop = icalproperty_new_due(icaltime_from_string(due));
    icalcomponent_add_property(data, prop);
  }
}
const char *task_data_get_start(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_DTSTART_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_start(TaskData *data, const char *start) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_DTSTART_PROPERTY);
  if (prop) {
    icalproperty_set_dtstart(prop, icaltime_from_string(start));
  } else {
    prop = icalproperty_new_dtstart(icaltime_from_string(start));
    icalcomponent_add_property(data, prop);
  }
}
const char *task_data_get_notes(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_DESCRIPTION_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_notes(TaskData *data, const char *notes) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_DESCRIPTION_PROPERTY);
  if (prop) {
    icalproperty_set_description(prop, notes);
  } else {
    prop = icalproperty_new_description(notes);
    icalcomponent_add_property(data, prop);
  }
}
const char *task_data_get_parent(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_RELATEDTO_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_parent(TaskData *data, const char *parent) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_RELATEDTO_PROPERTY);
  if (prop) {
    icalproperty_set_relatedto(prop, parent);
  } else {
    prop = icalproperty_new_relatedto(parent);
    icalcomponent_add_property(data, prop);
  }
}
const char *task_data_get_rrule(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_RRULE_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_rrule(TaskData *data, const char *rrule) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_RRULE_PROPERTY);
  if (prop) {
    icalproperty_set_rrule(prop, icalrecurrencetype_from_string(rrule));
  } else {
    prop = icalproperty_new_rrule(icalrecurrencetype_from_string(rrule));
    icalcomponent_add_property(data, prop);
  }
}
const char *task_data_get_text(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_SUMMARY_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_text(TaskData *data, const char *text) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_SUMMARY_PROPERTY);
  if (prop) {
    icalproperty_set_summary(prop, text);
  } else {
    prop = icalproperty_new_summary(text);
    icalcomponent_add_property(data, prop);
  }
}
const char *task_data_get_uid(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_UID_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_uid(TaskData *data, const char *uid) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_UID_PROPERTY);
  if (prop) {
    icalproperty_set_uid(prop, uid);
  } else {
    prop = icalproperty_new_uid(uid);
    icalcomponent_add_property(data, prop);
  }
}
const char *task_data_get_tags(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_CATEGORIES_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return "";
    return out;
  }
  return "";
}
void task_data_set_tags(TaskData *data, const char *tags) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_CATEGORIES_PROPERTY);
  if (prop) {
    icalproperty_set_categories(prop, tags);
  } else {
    prop = icalproperty_new_categories(tags);
    icalcomponent_add_property(data, prop);
  }
}
uint8_t task_data_get_percent(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_PERCENTCOMPLETE_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return atoi("0");
    return atoi(out);
  }
  return atoi("0");
}
void task_data_set_percent(TaskData *data, uint8_t percent) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_PERCENTCOMPLETE_PROPERTY);
  if (prop) {
    icalproperty_set_percentcomplete(prop, percent);
  } else {
    prop = icalproperty_new_percentcomplete(percent);
    icalcomponent_add_property(data, prop);
  }
}
uint8_t task_data_get_priority(TaskData *data) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_PRIORITY_PROPERTY);
  if (prop) {
    const char *out = icalproperty_get_value_as_string(prop);
    if (!out)
      return atoi("0");
    return atoi(out);
  }
  return atoi("0");
}
void task_data_set_priority(TaskData *data, uint8_t priority) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_PRIORITY_PROPERTY);
  if (prop) {
    icalproperty_set_priority(prop, priority);
  } else {
    prop = icalproperty_new_priority(priority);
    icalcomponent_add_property(data, prop);
  }
}
GStrv task_data_get_attachments(TaskData *data) {
  const char *prop = __get_x_prop_value(data, "X-ERRANDS-ATTACHMENTS", "");
  if (prop)
    return g_strsplit(prop, ",", -1);
  return NULL;
}
void task_data_set_attachments(TaskData *data, GStrv attachments) {
  g_autoptr(GString) str = g_string_new("");
  size_t len = g_strv_length(attachments);
  for (size_t i = 0; i < len; i++) {
    g_string_append(str, attachments[i]);
    if (i + 1 < len)
      g_string_append_c(str, ',');
  }
  __set_x_prop_value(data, "X-ERRANDS-ATTACHMENTS", str->str);
}
const char *task_data_get_color(TaskData *data) {
  return __get_x_prop_value(data, "X-ERRANDS-COLOR", "none");
}
void task_data_set_color(TaskData *data, const char *color) {
  __set_x_prop_value(data, "X-ERRANDS-COLOR", color);
};
const char *task_data_get_list_uid(TaskData *data) {
  return __get_x_prop_value(data, "X-ERRANDS-LIST-UID", "");
}
void task_data_set_list_uid(TaskData *data, const char *list_uid) {
  __set_x_prop_value(data, "X-ERRANDS-LIST-UID", list_uid);
}
bool task_data_get_deleted(TaskData *data) {
  return !strcmp(__get_x_prop_value(data, "X-ERRANDS-DELETED", "0"), "1") ? 1 : 0;
}
void task_data_set_deleted(TaskData *data, _Bool deleted) {
  __set_x_prop_value(data, "X-ERRANDS-DELETED", deleted ? "1" : "0");
}
bool task_data_get_expanded(TaskData *data) {
  return !strcmp(__get_x_prop_value(data, "X-ERRANDS-EXPANDED", "0"), "1") ? 1 : 0;
}
void task_data_set_expanded(TaskData *data, _Bool expanded) {
  __set_x_prop_value(data, "X-ERRANDS-EXPANDED", expanded ? "1" : "0");
}
bool task_data_get_notified(TaskData *data) {
  return !strcmp(__get_x_prop_value(data, "X-ERRANDS-NOTIFIED", "0"), "1") ? 1 : 0;
}
void task_data_set_notified(TaskData *data, _Bool notified) {
  __set_x_prop_value(data, "X-ERRANDS-NOTIFIED", notified ? "1" : "0");
}
bool task_data_get_synced(TaskData *data) {
  return !strcmp(__get_x_prop_value(data, "X-ERRANDS-SYNCED", "0"), "1") ? 1 : 0;
}
void task_data_set_synced(TaskData *data, _Bool synced) {
  __set_x_prop_value(data, "X-ERRANDS-SYNCED", synced ? "1" : "0");
}
bool task_data_get_toolbar_shown(TaskData *data) {
  return !strcmp(__get_x_prop_value(data, "X-ERRANDS-TOOLBAR-SHOWN", "0"), "1") ? 1 : 0;
}
void task_data_set_toolbar_shown(TaskData *data, _Bool toolbar_shown) {
  __set_x_prop_value(data, "X-ERRANDS-TOOLBAR-SHOWN", toolbar_shown ? "1" : "0");
}
bool task_data_get_trash(TaskData *data) {
  return !strcmp(__get_x_prop_value(data, "X-ERRANDS-TRASH", "0"), "1") ? 1 : 0;
}
void task_data_set_trash(TaskData *data, _Bool trash) {
  __set_x_prop_value(data, "X-ERRANDS-TRASH", trash ? "1" : "0");
}
bool task_data_get_completed(ListData *data) {
  return __get_prop_value_or(data, ICAL_COMPLETED_PROPERTY, NULL) ? true : false;
}
void task_data_set_completed(ListData *data, bool completed) {
  icalproperty *prop = icalcomponent_get_first_property(data, ICAL_COMPLETED_PROPERTY);
  char dt[17];
  get_date_time(dt);
  if (prop) {
    if (completed)
      icalproperty_set_completed(prop, icaltime_from_string(dt));
    else
      icalcomponent_remove_property(data, prop);
  } else {
    if (completed) {
      prop = icalproperty_new_completed(icaltime_from_string(dt));
      icalcomponent_add_property(data, prop);
    }
  }
}

// --- LOADING --- //

static gint list_sort_by_position_func(gconstpointer a, gconstpointer b) {
  ListData *d1 = *(ListData **)a;
  ListData *d2 = *(ListData **)b;
  return list_data_get_position(d1) > list_data_get_position(d2);
}

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
    const char *color = cJSON_GetObjectItem(cal_item, "color")->valuestring;
    const bool deleted = (bool)cJSON_GetObjectItem(cal_item, "deleted")->valueint;
    const bool synced = (bool)cJSON_GetObjectItem(cal_item, "synced")->valueint;
    const char *name = cJSON_GetObjectItem(cal_item, "name")->valuestring;
    const char *list_uid = cJSON_GetObjectItem(cal_item, "uid")->valuestring;
    ListData *calendar =
        list_data_new(list_uid, name, !strcmp(color, "") ? "none" : color, deleted, synced, i);

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

void errands_data_load_lists() {
  user_dir = g_build_path(PATH_SEP, g_get_user_data_dir(), "errands", NULL);
  if (!directory_exists(user_dir)) {
    LOG("User Data: Creating user data directory at %s", user_dir);
    g_mkdir_with_parents(user_dir, 0755);
  }
  LOG("User Data: Loading at %s", user_dir);
  errands_data_migrate();
  ldata = g_ptr_array_new();
  tdata = g_ptr_array_new();
  g_autoptr(GDir) dir = g_dir_open(user_dir, 0, NULL);
  if (!dir)
    return;
  const char *filename;
  while ((filename = g_dir_read_name(dir))) {
    char *is_ics = strstr(filename, ".ics");
    if (is_ics) {
      g_autofree gchar *path = g_build_path(PATH_SEP, user_dir, filename, NULL);
      g_autofree gchar *content = read_file_to_string(path);
      if (content) {
        LOG("User Data: Loading calendar %s", path);
        icalcomponent *calendar = icalparser_parse_string(content);
        if (calendar) {
          // Delete file if calendar deleted and sync is off
          if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b &&
              list_data_get_deleted(calendar)) {
            LOG("User Data: Calendar was deleted. Removing %s", path);
            icalcomponent_free(calendar);
            remove(path);
            continue;
          }
          g_ptr_array_add(ldata, calendar);
          // Load tasks
          icalcomponent *c;
          for (c = icalcomponent_get_first_component(calendar, ICAL_VTODO_COMPONENT); c != 0;
               c = icalcomponent_get_next_component(calendar, ICAL_VTODO_COMPONENT))
            g_ptr_array_add(tdata, c);
        }
      }
    }
  }
  g_ptr_array_sort(ldata, list_sort_by_position_func);
  list_data_update_positions();
}

void errands_data_write_list(ListData *data) {
  g_autofree gchar *filename = g_strdup_printf("%s.ics", list_data_get_uid(data));
  g_autofree gchar *path = g_build_path(PATH_SEP, user_dir, filename, NULL);
  const char *ical_string = icalcomponent_as_ical_string(data);
  write_string_to_file(path, ical_string);
  LOG("User Data: Save list %s", path);
}
