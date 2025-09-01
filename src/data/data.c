#include "data.h"
#include "../utils.h"

GHashTable *ldata = NULL;
GHashTable *tdata = NULL;

void errands_data_free(icalcomponent *data) { icalcomponent_free(data); }

// --- LIST DATA --- //

ListData *list_data_new(const char *uid, const char *name, const char *color, bool deleted, bool synced, int position) {
  tb_log("List Data: Create '%s'", name);
  icalcomponent *calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
  icalcomponent_add_property(calendar, icalproperty_new_version("2.0"));
  icalcomponent_add_property(calendar, icalproperty_new_prodid("~//Errands"));
  if (!uid) {
    g_autofree gchar *_uid = g_uuid_string_random();
    errands_data_set_str(calendar, DATA_PROP_LIST_UID, _uid);
  } else errands_data_set_str(calendar, DATA_PROP_LIST_UID, uid);
  errands_data_set_str(calendar, DATA_PROP_LIST_NAME, name);
  if (!color) errands_data_set_str(calendar, DATA_PROP_COLOR, generate_hex());
  errands_data_set_bool(calendar, DATA_PROP_DELETED, deleted);
  errands_data_set_bool(calendar, DATA_PROP_SYNCED, synced);
  tb_log("List Data: Created");
  return calendar;
}

ListData *list_data_new_from_ical(const char *ical, const char *uid, size_t position) {
  icalcomponent *calendar = icalcomponent_new_from_string(ical);
  errands_data_set_str(calendar, DATA_PROP_LIST_UID, uid);
  return calendar;
}

GPtrArray *list_data_get_tasks(ListData *data) {
  GPtrArray *tasks = g_ptr_array_new();
  icalcomponent *c;
  for (c = icalcomponent_get_first_component(data, ICAL_VTODO_COMPONENT); c != 0;
       c = icalcomponent_get_next_component(data, ICAL_VTODO_COMPONENT))
    g_ptr_array_add(tasks, c);
  return tasks;
}

TaskData *list_data_create_task(ListData *list, const char *text, const char *list_uid, const char *parent) {
  TaskData *task_data = icalcomponent_new(ICAL_VTODO_COMPONENT);
  g_autofree gchar *uid = g_uuid_string_random();
  errands_data_set_str(task_data, DATA_PROP_UID, uid);
  errands_data_set_str(task_data, DATA_PROP_TEXT, text);
  if (strcmp(parent, "")) errands_data_set_str(task_data, DATA_PROP_PARENT, parent);
  errands_data_set_str(task_data, DATA_PROP_LIST_UID, list_uid);
  errands_data_set_time(task_data, DATA_PROP_CREATED_TIME, icaltime_get_date_time_now());
  icalcomponent_add_component(list, task_data);
  return task_data;
}

gchar *list_data_print(ListData *data) {
  const char *name = errands_data_get_str(data, DATA_PROP_LIST_NAME);
  size_t len = strlen(name);
  char *list_name = strndup(name, len > 72 ? 72 : len);
  // Print list name
  GString *out = g_string_new(list_name);
  g_string_append(out, "\n\n");
  // Print tasks
  GPtrArray *tasks = list_data_get_tasks(data);
  for (size_t i = 0; i < tasks->len; i++) task_data_print(tasks->pdata[i], out, 0);
  free(list_name);
  g_ptr_array_free(tasks, false);
  return g_string_free(out, false);
}

// --- TASK DATA --- //

TaskData *task_data_new(ListData *list, const char *text, const char *list_uid) {
  icalcomponent *task = icalcomponent_new(ICAL_VTODO_COMPONENT);
  return task;
}

void task_data_free(TaskData *data) { free(data); }

ListData *task_data_get_list(TaskData *data) { return icalcomponent_get_parent(data); }

GPtrArray *task_data_get_children(TaskData *data) {
  const char *uid = errands_data_get_str(data, DATA_PROP_UID);
  ListData *list = task_data_get_list(data);
  GPtrArray *tasks = list_data_get_tasks(list);
  GPtrArray *children = g_ptr_array_new();
  for (size_t i = 0; i < tasks->len; i++)
    if (!strcmp(uid, errands_data_get_str(tasks->pdata[i], DATA_PROP_PARENT)))
      g_ptr_array_add(children, tasks->pdata[i]);
  g_ptr_array_free(tasks, false);
  return children;
}

void task_data_print(TaskData *data, GString *out, size_t indent) {
  const uint8_t max_line_len = 72;
  for (size_t i = 0; i < indent; i++) g_string_append(out, "  ");
  g_string_append_printf(out, "[%s] ",
                         !icaltime_is_null_time(errands_data_get_time(data, DATA_PROP_COMPLETED_TIME)) ? "x" : " ");
  const char *text = errands_data_get_str(data, DATA_PROP_TEXT);
  size_t count = 0;
  char c = text[0];
  while (c != '\0') {
    g_string_append_c(out, c);
    count++;
    c = text[count];
    if (count % max_line_len == 0) {
      g_string_append_c(out, '\n');
      for (size_t i = 0; i < indent; i++) g_string_append(out, "  ");
      g_string_append(out, "    ");
    }
  }
  g_string_append_c(out, '\n');
  GPtrArray *children = task_data_get_children(data);
  indent++;
  for (size_t i = 0; i < children->len; i++) task_data_print(children->pdata[i], out, indent);
}

bool icaltime_is_null_date(const struct icaltimetype t) { return t.year == 0 && t.month == 0 && t.day == 0; }
icaltimetype icaltime_merge_date_and_time(const struct icaltimetype date, const struct icaltimetype time) {
  icaltimetype result = date;
  result.hour = time.hour;
  result.minute = time.minute;
  result.second = time.second;
  result.is_date = false;
  return result;
}
icaltimetype icaltime_get_date_time_now() {
  g_autoptr(GDateTime) dt = g_date_time_new_now_local();
  icaltimetype dt_now = icaltime_today();
  dt_now.is_date = false;
  dt_now.hour = g_date_time_get_hour(dt);
  dt_now.minute = g_date_time_get_minute(dt);
  dt_now.second = g_date_time_get_second(dt);
  return dt_now;
}

bool icalrecurrencetype_compare(const struct icalrecurrencetype *a, const struct icalrecurrencetype *b) {
  if (a->freq != b->freq) return false;
  if (a->count != b->count) return false;
  if (a->interval != b->interval) return false;
  if (a->week_start != b->week_start) return false;
  if (icaltime_compare(a->until, b->until) != 0) return false;
  if (memcmp(a->by_second, b->by_second, ICAL_BY_SECOND_SIZE * sizeof(short)) != 0) return false;
  if (memcmp(a->by_minute, b->by_minute, ICAL_BY_MINUTE_SIZE * sizeof(short)) != 0) return false;
  if (memcmp(a->by_hour, b->by_hour, ICAL_BY_HOUR_SIZE * sizeof(short)) != 0) return false;
  if (memcmp(a->by_day, b->by_day, ICAL_BY_DAY_SIZE * sizeof(short)) != 0) return false;
  if (memcmp(a->by_month_day, b->by_month_day, ICAL_BY_MONTHDAY_SIZE * sizeof(short)) != 0) return false;
  if (memcmp(a->by_year_day, b->by_year_day, ICAL_BY_YEARDAY_SIZE * sizeof(short)) != 0) return false;
  if (memcmp(a->by_week_no, b->by_week_no, ICAL_BY_WEEKNO_SIZE * sizeof(short)) != 0) return false;
  if (memcmp(a->by_month, b->by_month, ICAL_BY_MONTH_SIZE * sizeof(short)) != 0) return false;
  if (memcmp(a->by_set_pos, b->by_set_pos, ICAL_BY_SETPOS_SIZE * sizeof(short)) != 0) return false;
  if ((a->rscale == NULL && b->rscale != NULL) || (a->rscale != NULL && b->rscale == NULL)) return false;
  if (a->rscale != NULL && b->rscale != NULL && strcmp(a->rscale, b->rscale) != 0) return false;
  if (a->skip != b->skip) return false;
  return true;
}
