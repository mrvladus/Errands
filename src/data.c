#include "data.h"
#include "glib.h"
#include "settings.h"

#include "vendor/json.h"
#include "vendor/toolbox.h"

#include <assert.h>
#include <libical/ical.h>

AUTOPTR_DEFINE(JSON, json_free)

// ---------- GLOBALS ---------- //

GPtrArray *errands_data_lists;
static char *user_dir;

// ---------- LOADING DATA ---------- //

static void errands_data_migrate_from_46() {
  g_autofree gchar *old_data_file = g_build_filename(user_dir, "data.json", NULL);
  if (!g_file_test(old_data_file, G_FILE_TEST_EXISTS)) return;
  LOG("User Data: Migrate from 46.x");
  autofree char *contents = read_file_to_string(old_data_file);
  // Read file contents
  if (!contents) {
    LOG("User Data: Failed to read old data file at %s", old_data_file);
    return;
  }
  // Parse JSON
  autoptr(JSON) root = json_parse(contents);
  if (!root) return;
  // Process lists
  JSON *cal_arr = json_object_get(root, "lists");
  JSON *cal_item = NULL;
  for (cal_item = cal_arr->child; cal_item; cal_item = cal_item->next) {
    JSON *color_item = json_object_get(cal_item, "color");
    JSON *deleted_item = json_object_get(cal_item, "deleted");
    JSON *synced_item = json_object_get(cal_item, "synced");
    JSON *name_item = json_object_get(cal_item, "name");
    JSON *list_uid_item = json_object_get(cal_item, "uid");

    autoptr(ListData) calendar =
        errands_list_data_create(list_uid_item->string_val, name_item->string_val, color_item->string_val,
                                 deleted_item->bool_val, synced_item->bool_val);

    // Process tasks
    JSON *tasks_arr = json_object_get(root, "tasks");
    if (tasks_arr->type != JSON_TYPE_ARRAY) continue;
    for (JSON *task_item = tasks_arr->child; task_item; task_item = task_item->next) {
      JSON *task_list_uid_item = json_object_get(task_item, "list_uid");
      const gchar *task_list_uid = task_list_uid_item ? task_list_uid_item->string_val : NULL;
      if (!task_list_uid || !STR_EQUAL(task_list_uid, list_uid_item->string_val)) continue;
      // Process attachments
      JSON *task_attachments_arr = json_object_get(task_item, "attachments");
      g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
      for (JSON *attachment_item = task_attachments_arr->child; attachment_item;
           attachment_item = attachment_item->next) {
        g_strv_builder_add(builder, attachment_item->string_val);
      }
      g_auto(GStrv) attachments = g_strv_builder_end(builder);
      // Process tags
      JSON *task_tags_arr = json_object_get(task_item, "tags");
      g_autoptr(GStrvBuilder) tags_builder = g_strv_builder_new();
      JSON *tags_item = NULL;
      for (tags_item = task_tags_arr->child; tags_item; tags_item = tags_item->next)
        g_strv_builder_add(builder, tags_item->string_val);
      g_auto(GStrv) tags = g_strv_builder_end(builder);
      // Extract task properties
      JSON *color_item = json_object_get(task_item, "color");
      JSON *completed_item = json_object_get(task_item, "completed");
      JSON *changed_at_item = json_object_get(task_item, "changed_at");
      JSON *created_at_item = json_object_get(task_item, "created_at");
      JSON *deleted_item = json_object_get(task_item, "deleted");
      JSON *due_date_item = json_object_get(task_item, "due_date");
      JSON *expanded_item = json_object_get(task_item, "expanded");
      JSON *notes_item = json_object_get(task_item, "notes");
      JSON *notified_item = json_object_get(task_item, "notified");
      JSON *parent_item = json_object_get(task_item, "parent");
      JSON *percent_complete_item = json_object_get(task_item, "percent_complete");
      JSON *priority_item = json_object_get(task_item, "priority");
      JSON *rrule_item = json_object_get(task_item, "rrule");
      JSON *start_date_item = json_object_get(task_item, "start_date");
      JSON *synced_item = json_object_get(task_item, "synced");
      JSON *text_item = json_object_get(task_item, "text");
      JSON *toolbar_shown_item = json_object_get(task_item, "toolbar_shown");
      JSON *trash_item = json_object_get(task_item, "trash");
      JSON *uid_item = json_object_get(task_item, "uid");
      // Create iCalendar event
      icalcomponent *event = icalcomponent_new(ICAL_VTODO_COMPONENT);
      if (completed_item) errands_data_set_time(event, DATA_PROP_COMPLETED_TIME, icaltime_get_date_time_now());
      if (tags) errands_data_set_strv(event, DATA_PROP_TAGS, tags);
      if (changed_at_item)
        errands_data_set_time(event, DATA_PROP_CHANGED_TIME, icaltime_from_string(changed_at_item->string_val));
      if (created_at_item)
        errands_data_set_time(event, DATA_PROP_CREATED_TIME, icaltime_from_string(created_at_item->string_val));
      if (due_date_item)
        errands_data_set_time(event, DATA_PROP_DUE_TIME, icaltime_from_string(due_date_item->string_val));
      if (notes_item) errands_data_set_str(event, DATA_PROP_NOTES, notes_item->string_val);
      if (parent_item) errands_data_set_str(event, DATA_PROP_PARENT, parent_item->string_val);
      if (percent_complete_item) errands_data_set_int(event, DATA_PROP_PERCENT, percent_complete_item->int_val);
      if (priority_item) errands_data_set_int(event, DATA_PROP_PRIORITY, priority_item->int_val);
      if (rrule_item) errands_data_set_str(event, DATA_PROP_RRULE, rrule_item->string_val);
      if (start_date_item)
        errands_data_set_time(event, DATA_PROP_START_TIME, icaltime_from_string(start_date_item->string_val));
      if (text_item) errands_data_set_str(event, DATA_PROP_TEXT, text_item->string_val);
      if (uid_item) errands_data_set_str(event, DATA_PROP_UID, uid_item->string_val);
      errands_data_set_strv(event, DATA_PROP_ATTACHMENTS, attachments);
      errands_data_set_str(event, DATA_PROP_COLOR, color_item->string_val);
      errands_data_set_bool(event, DATA_PROP_DELETED, deleted_item->bool_val);
      errands_data_set_bool(event, DATA_PROP_EXPANDED, expanded_item->bool_val);
      errands_data_set_bool(event, DATA_PROP_NOTIFIED, notified_item->bool_val);
      errands_data_set_bool(event, DATA_PROP_SYNCED, synced_item->bool_val);
      errands_data_set_bool(event, DATA_PROP_TOOLBAR_SHOWN, toolbar_shown_item->bool_val);
      errands_data_set_bool(event, DATA_PROP_TRASH, trash_item->bool_val);
      errands_data_set_str(event, DATA_PROP_LIST_UID, task_list_uid_item->string_val);
      icalcomponent_add_component(calendar->data, event);
    }
    // Save calendar to file
    const char *calendar_filename = tmp_str_printf("%s.ics", list_uid_item->string_val);
    g_autofree gchar *calendar_file_path = g_build_filename(user_dir, calendar_filename, NULL);
    bool res = write_string_to_file(calendar_file_path, icalcomponent_as_ical_string(calendar->data));
    if (!res) LOG("User Data: Failed to save calendar to %s", calendar_file_path);
  }
  remove(old_data_file);
}

static void collect_and_sort_children_recursive(TaskData *parent, GPtrArray *all_tasks) {
  const char *uid = errands_data_get_str(parent->data, DATA_PROP_UID);
  for_range(i, 0, all_tasks->len) {
    icalcomponent *comp = TASK_DATA(all_tasks->pdata[i])->data;
    const char *parent_uid = errands_data_get_str(comp, DATA_PROP_PARENT);
    if (parent_uid && STR_EQUAL(uid, parent_uid)) {
      TaskData *child = errands_task_data_new(comp, parent, parent->list);
      g_ptr_array_add(parent->children, child);
      collect_and_sort_children_recursive(child, all_tasks);
    }
  }
  g_ptr_array_sort_values(parent->children, errands_data_sort_func);
}

void errands_data_init() {
  errands_data_lists = g_ptr_array_new_with_free_func((GDestroyNotify)errands_list_data_free);
  user_dir = g_build_filename(g_get_user_data_dir(), "errands", NULL);

  g_mkdir_with_parents(user_dir, 0755);
  errands_data_migrate_from_46();

  // Load lists
  g_autoptr(GDir) dir = g_dir_open(user_dir, 0, NULL);
  if (!dir) return;
  const char *filename;
  TIMER_START;
  while ((filename = g_dir_read_name(dir))) {
    if (!g_str_has_suffix(filename, ".ics")) continue;
    g_autofree gchar *path = g_build_filename(user_dir, filename, NULL);
    g_autofree gchar *content = read_file_to_string(path);
    if (!content) continue;
    icalcomponent *calendar = icalparser_parse_string(content);
    if (!calendar) continue;
    // Delete file if calendar deleted and sync is off
    if (!errands_settings_get(SETTING_SYNC).b && errands_data_get_bool(calendar, DATA_PROP_DELETED)) {
      LOG("User Data: Calendar was deleted. Removing %s", path);
      icalcomponent_free(calendar);
      remove(path);
      continue;
    }
    g_ptr_array_add(errands_data_lists, errands_list_data_new(calendar));
    LOG("User Data: Loaded calendar %s", path);
  }

  // Load tasks

  // Collect all tasks and add toplevel tasks to lists
  g_autoptr(GPtrArray) all_tasks = g_ptr_array_new();
  for_range(i, 0, errands_data_lists->len) {
    ListData *data = g_ptr_array_index(errands_data_lists, i);
    for (icalcomponent *c = icalcomponent_get_first_component(data->data, ICAL_VTODO_COMPONENT); c != 0;
         c = icalcomponent_get_next_component(data->data, ICAL_VTODO_COMPONENT)) {
      if (errands_data_get_bool(c, DATA_PROP_DELETED)) continue;
      TaskData *task_data = errands_task_data_new(c, NULL, data);
      g_ptr_array_add(all_tasks, task_data);
      if (errands_data_get_str(c, DATA_PROP_PARENT)) continue;
      g_ptr_array_add(data->children, task_data);
    }
    g_ptr_array_sort_values(data->children, errands_data_sort_func);
  }

  // Collect children recursively
  for_range(i, 0, errands_data_lists->len) {
    ListData *data = g_ptr_array_index(errands_data_lists, i);
    for_range(j, 0, data->children->len) {
      TaskData *toplevel_task = g_ptr_array_index(data->children, j);
      collect_and_sort_children_recursive(toplevel_task, all_tasks);
    }
  }

  LOG("User Data: Loaded %d tasks from %d lists in %f sec.", all_tasks->len, errands_data_lists->len, TIMER_ELAPSED_MS);
}

void errands_data_get_stats(size_t *total, size_t *completed) {
  for_range(i, 0, errands_data_lists->len) {
    ListData *data = g_ptr_array_index(errands_data_lists, i);
    CONTINUE_IF(errands_data_get_bool(data->data, DATA_PROP_DELETED))
    errands_list_data_get_stats(data, total, completed);
  }
}

void errands_data_get_flat_list(GPtrArray *tasks) {
  for_range(i, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, i);
    errands_list_data_get_flat_list(list, tasks);
  }
}

void errands_data_sort() {
  for_range(i, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, i);
    errands_list_data_sort(list);
  }
}

// ---------- LIST DATA ---------- //

ListData *errands_list_data_new(icalcomponent *data) {
  ListData *list = malloc(sizeof(ListData));
  list->data = data;
  list->children = g_ptr_array_new_with_free_func((GDestroyNotify)errands_task_data_free);
  return list;
}

ListData *errands_list_data_create(const char *uid, const char *name, const char *color, bool deleted, bool synced) {
  if (!name) return NULL;
  icalcomponent *calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
  icalcomponent_add_property(calendar, icalproperty_new_version("2.0"));
  icalcomponent_add_property(calendar, icalproperty_new_prodid("~//Errands"));
  if (!uid) {
    g_autofree gchar *_uid = g_uuid_string_random();
    errands_data_set_str(calendar, DATA_PROP_LIST_UID, _uid);
  } else errands_data_set_str(calendar, DATA_PROP_LIST_UID, uid);
  errands_data_set_str(calendar, DATA_PROP_LIST_NAME, name);
  if (!color) errands_data_set_str(calendar, DATA_PROP_COLOR, generate_hex_as_str());
  errands_data_set_bool(calendar, DATA_PROP_DELETED, deleted);
  errands_data_set_bool(calendar, DATA_PROP_SYNCED, synced);
  ListData *data = errands_list_data_new(calendar);
  LOG("Data: Created '%s'", errands_data_get_str(calendar, DATA_PROP_LIST_UID));
  return data;
}

void errands_list_data_free(ListData *data) {
  if (!data) return;
  if (data->data) icalcomponent_free(data->data);
  g_ptr_array_free(data->children, true);
  free(data);
}

static void errands_list_data_sort_recursive(GPtrArray *array) {
  g_ptr_array_sort_values(array, errands_data_sort_func);
  for_range(i, 0, array->len) {
    TaskData *task_data = g_ptr_array_index(array, i);
    errands_list_data_sort_recursive(task_data->children);
  }
}

void errands_list_data_sort(ListData *data) {
  g_ptr_array_sort_values(data->children, errands_data_sort_func);
  for_range(i, 0, data->children->len) {
    TaskData *task_data = g_ptr_array_index(data->children, i);
    errands_list_data_sort_recursive(task_data->children);
  }
}

void errands_list_data_get_stats(ListData *data, size_t *total, size_t *completed) {
  for_range(i, 0, data->children->len) {
    TaskData *child = g_ptr_array_index(data->children, i);
    bool deleted = errands_data_get_bool(child->data, DATA_PROP_DELETED);
    bool trash = errands_data_get_bool(child->data, DATA_PROP_TRASH);
    CONTINUE_IF(deleted || trash);
    bool is_completed = !icaltime_is_null_date(errands_data_get_time(child->data, DATA_PROP_COMPLETED_TIME));
    if (is_completed) (*completed)++;
    (*total)++;
    errands_task_data_get_stats_recursive(child, total, completed);
  }
}

GPtrArray *errands_list_data_get_all_tasks_as_icalcomponents(ListData *data) {
  if (icalcomponent_isa(data->data) != ICAL_VCALENDAR_COMPONENT) return NULL;
  GPtrArray *tasks = g_ptr_array_new();
  for (icalcomponent *c = icalcomponent_get_first_component(data->data, ICAL_VTODO_COMPONENT); c != 0;
       c = icalcomponent_get_next_component(data->data, ICAL_VTODO_COMPONENT))
    g_ptr_array_add(tasks, c);
  return tasks;
}

void errands_list_data_print(ListData *data) {
  for_range(i, 0, data->children->len) {
    TaskData *task_data = g_ptr_array_index(data->children, i);
    errands_task_data_print(task_data);
  }
}

void errands_list_data_get_flat_list(ListData *data, GPtrArray *tasks) {
  for_range(i, 0, data->children->len) {
    TaskData *task_data = g_ptr_array_index(data->children, i);
    g_ptr_array_add(tasks, task_data);
    errands_task_data_get_flat_list(task_data, tasks);
  }
}

static void errands_data__write_list(ListData *data) {
  if (!data || !data->data) return;
  g_autofree gchar *path = g_strdup_printf("%s/%s.ics", user_dir, errands_data_get_str(data->data, DATA_PROP_LIST_UID));
  if (!g_file_set_contents(path, icalcomponent_as_ical_string(data->data), -1, NULL)) {
    LOG("User Data: Failed to save list '%s'", path);
    return;
  }
  LOG("User Data: Saved list '%s'", path);
}

void errands_data_write_list(ListData *data) { g_idle_add_once((GSourceOnceFunc)errands_data__write_list, data); }

// ---------- TASK DATA ---------- //

TaskData *errands_task_data_new(icalcomponent *data, TaskData *parent, ListData *list) {
  TaskData *task = malloc(sizeof(TaskData));
  task->data = data;
  task->parent = parent;
  task->list = list;
  task->children = g_ptr_array_new_with_free_func((GDestroyNotify)errands_task_data_free);
  return task;
}

TaskData *errands_task_data_create_task(ListData *list, TaskData *parent, const char *text) {
  icalcomponent *task_data = icalcomponent_new(ICAL_VTODO_COMPONENT);
  g_autofree gchar *uid = g_uuid_string_random();
  errands_data_set_str(task_data, DATA_PROP_UID, uid);
  errands_data_set_str(task_data, DATA_PROP_TEXT, text);
  const char *list_uid = errands_data_get_str(list->data, DATA_PROP_LIST_UID);
  errands_data_set_str(task_data, DATA_PROP_LIST_UID, list_uid);
  if (parent) {
    const char *parent_uid = errands_data_get_str(parent->data, DATA_PROP_UID);
    errands_data_set_str(task_data, DATA_PROP_PARENT, parent_uid);
  }
  errands_data_set_time(task_data, DATA_PROP_CREATED_TIME, icaltime_get_date_time_now());
  icalcomponent_add_component(list->data, task_data);
  TaskData *task = errands_task_data_new(task_data, parent, list);
  return task;
}

void errands_task_data_free(TaskData *data) {
  if (!data) return;
  g_ptr_array_free(data->children, true);
  free(data);
}

size_t errands_task_data_get_indent_level(TaskData *data) {
  if (!data) return 0;
  size_t indent = 0;
  TaskData *parent = data->parent;
  while (parent) {
    indent++;
    parent = parent->parent;
  }
  return indent;
}

void errands_task_data_get_stats_recursive(TaskData *data, size_t *total, size_t *completed) {
  for_range(i, 0, data->children->len) {
    TaskData *child = g_ptr_array_index(data->children, i);
    bool deleted = errands_data_get_bool(child->data, DATA_PROP_DELETED);
    bool trash = errands_data_get_bool(child->data, DATA_PROP_TRASH);
    CONTINUE_IF(deleted || trash);
    bool is_completed = !icaltime_is_null_date(errands_data_get_time(child->data, DATA_PROP_COMPLETED_TIME));
    if (is_completed) (*completed)++;
    (*total)++;
    errands_task_data_get_stats_recursive(child, total, completed);
  }
}

void errands_task_data_print(TaskData *data) {
  const char *text = errands_data_get_str(data->data, DATA_PROP_TEXT);
  bool completed = !icaltime_is_null_date(errands_data_get_time(data->data, DATA_PROP_COMPLETED_TIME));
  LOG("[%s] %s", completed ? "x" : " ", text);
}

void errands_task_data_get_flat_list(TaskData *parent, GPtrArray *array) {
  for_range(i, 0, parent->children->len) {
    TaskData *sub_task = g_ptr_array_index(parent->children, i);
    g_ptr_array_add(array, sub_task);
    errands_task_data_get_flat_list(sub_task, array);
  }
}

// ---------- SAVING DATA ---------- //

// ---------- CLEANUP ---------- //

void errands_data_cleanup(void) {
  g_free(user_dir);
  g_ptr_array_free(errands_data_lists, true);
}

// ---------- LIST DATA ---------- //

// GPtrArray *list_data_get_tasks(ListData *data) {
//   GPtrArray *tasks = g_ptr_array_new();
//   for (icalcomponent *c = icalcomponent_get_first_component(data, ICAL_VTODO_COMPONENT); c != 0;
//        c = icalcomponent_get_next_component(data, ICAL_VTODO_COMPONENT))
//     g_ptr_array_add(tasks, c);
//   return tasks;
// }

// TaskData *list_data_create_task(ListData *list, const char *text, const char *list_uid, const char *parent) {
//   TaskData *task_data = icalcomponent_new(ICAL_VTODO_COMPONENT);
//   g_autofree gchar *uid = g_uuid_string_random();
//   errands_data_set_str(task_data, DATA_PROP_UID, uid);
//   errands_data_set_str(task_data, DATA_PROP_TEXT, text);
//   if (!STR_EQUAL(parent, "")) errands_data_set_str(task_data, DATA_PROP_PARENT, parent);
//   errands_data_set_str(task_data, DATA_PROP_LIST_UID, list_uid);
//   errands_data_set_time(task_data, DATA_PROP_CREATED_TIME, icaltime_get_date_time_now());
//   icalcomponent_add_component(list, task_data);
//   return task_data;
// }

// gchar *list_data_print(ListData *data) {
//   const char *name = errands_data_get_str(data, DATA_PROP_LIST_NAME);
//   size_t len = strlen(name);
//   g_autofree gchar *list_name = g_strndup(name, len > 72 ? 72 : len);
//   // Print list name
//   GString *out = g_string_new(list_name);
//   g_string_append(out, "\n\n");
//   // Print tasks
//   GPtrArray *tasks = list_data_get_tasks(data);
//   for (size_t i = 0; i < tasks->len; i++) task_data_print(tasks->pdata[i], out, 0);
//   g_ptr_array_free(tasks, false);
//   return g_string_free(out, false);
// }

// ---------- TASK DATA ---------- //

// TaskData *task_data_new(ListData *list, const char *text, const char *list_uid) {
//   icalcomponent *task = icalcomponent_new(ICAL_VTODO_COMPONENT);
//   return task;
// }

// void task_data_free(TaskData *data) { free(data); }

// ListData *task_data_get_list(TaskData *data) { return icalcomponent_get_parent(data); }

// GPtrArray *errands_task_data_get_children(TaskData *data) {
//   const char *uid = errands_data_get_str(data, DATA_PROP_UID);
//   g_autoptr(GPtrArray) tasks = list_data_get_tasks(task_data_get_list(data));
//   GPtrArray *children = g_ptr_array_new();
//   for (size_t i = 0; i < tasks->len; i++) {
//     TaskData *td = tasks->pdata[i];
//     const char *parent_uid = errands_data_get_str(td, DATA_PROP_PARENT);
//     if (parent_uid && STR_EQUAL(uid, parent_uid)) g_ptr_array_add(children, td);
//   }
//   return children;
// }

// void task_data_get_sub_tasks_tree(TaskData *data, GPtrArray *arr, bool sort) {
//   g_autoptr(GPtrArray) sub_tasks = errands_task_data_get_children(data);
//   for (size_t i = 0; i < sub_tasks->len; i++) {
//     TaskData *child = sub_tasks->pdata[i];
//     g_ptr_array_add(arr, child);
//     task_data_get_sub_tasks_tree(child, arr, sort);
//   }
// }

// ---------- PRINTING ---------- //

// void task_data_print(TaskData *data, GString *out, size_t indent) {
//   const uint8_t max_line_len = 72;
//   for (size_t i = 0; i < indent; i++) g_string_append(out, "  ");
//   g_string_append_printf(out, "[%s] ",
//                          !icaltime_is_null_time(errands_data_get_time(data, DATA_PROP_COMPLETED_TIME)) ? "x" : " ");
//   const char *text = errands_data_get_str(data, DATA_PROP_TEXT);
//   size_t count = 0;
//   char c = text[0];
//   while (c != '\0') {
//     g_string_append_c(out, c);
//     count++;
//     c = text[count];
//     if (count % max_line_len == 0) {
//       g_string_append_c(out, '\n');
//       for (size_t i = 0; i < indent; i++) g_string_append(out, "  ");
//       g_string_append(out, "    ");
//     }
//   }
//   g_string_append_c(out, '\n');
//   GPtrArray *children = errands_task_data_get_children(data);
//   indent++;
//   for (size_t i = 0; i < children->len; i++) task_data_print(children->pdata[i], out, indent);
// }

// ---------- ICAL UTILS ---------- //

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

// ---------- SORT AND FILTER FUNCTIONS ---------- //

gint errands_data_sort_func(gconstpointer a, gconstpointer b) {
  if (!a || !b) return 0;
  TaskData *td_a = (TaskData *)a;
  TaskData *td_b = (TaskData *)b;
  icalcomponent *data_a = td_a->data;
  icalcomponent *data_b = td_b->data;

  // LOG("Text %s %s", errands_data_get_str(data_a, DATA_PROP_TEXT), errands_data_get_str(data_b, DATA_PROP_TEXT));

  // Completion sort first
  gboolean completed_a = !icaltime_is_null_date(errands_data_get_time(data_a, DATA_PROP_COMPLETED_TIME));
  gboolean completed_b = !icaltime_is_null_date(errands_data_get_time(data_b, DATA_PROP_COMPLETED_TIME));
  if (completed_a != completed_b) return completed_a - completed_b;

  // Then apply global sort
  switch (errands_settings_get(SETTING_SORT_BY).i) {
  case SORT_TYPE_CREATION_DATE: {
    icaltimetype creation_date_a = errands_data_get_time(data_a, DATA_PROP_CREATED_TIME);
    icaltimetype creation_date_b = errands_data_get_time(data_b, DATA_PROP_CREATED_TIME);
    return icaltime_compare(creation_date_b, creation_date_a);
  }
  case SORT_TYPE_DUE_DATE: {
    icaltimetype due_a = errands_data_get_time(data_a, DATA_PROP_DUE_TIME);
    icaltimetype due_b = errands_data_get_time(data_b, DATA_PROP_DUE_TIME);
    bool null_a = icaltime_is_null_time(due_a);
    bool null_b = icaltime_is_null_time(due_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(due_a, due_b);
  }
  case SORT_TYPE_PRIORITY: {
    int p_a = errands_data_get_int(data_a, DATA_PROP_PRIORITY);
    int p_b = errands_data_get_int(data_b, DATA_PROP_PRIORITY);
    return p_b - p_a;
  }
  case SORT_TYPE_START_DATE: {
    icaltimetype start_a = errands_data_get_time(data_a, DATA_PROP_START_TIME);
    icaltimetype start_b = errands_data_get_time(data_b, DATA_PROP_START_TIME);
    bool null_a = icaltime_is_null_time(start_a);
    bool null_b = icaltime_is_null_time(start_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(start_a, start_b);
  }
  default: return 0;
  }
}

// ---------- PROPERTIES FUNCTIONS ---------- //

// --- UTILS --- //

static icalproperty *get_x_prop(icalcomponent *ical, const char *xprop, const char *default_val) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_X_PROPERTY);
  while (property) {
    const char *name = icalproperty_get_x_name(property);
    if (name && !strcmp(name, xprop)) return property;
    property = icalcomponent_get_next_property(ical, ICAL_X_PROPERTY);
  }
  if (!default_val) return NULL;
  property = icalproperty_new_x(default_val);
  icalproperty_set_x_name(property, xprop);
  icalcomponent_add_property(ical, property);
  return property;
}

static const char *get_x_prop_value(icalcomponent *ical, const char *xprop, const char *default_val) {
  icalproperty *property = get_x_prop(ical, xprop, default_val);
  if (!property) return NULL;
  return icalproperty_get_value_as_string(property);
}

static void set_x_prop_value(icalcomponent *ical, const char *xprop, const char *val) {
  icalproperty *property = get_x_prop(ical, xprop, val);
  icalproperty_set_x(property, val);
}

// --- GETTERS --- //

const char *errands_data_get_str(icalcomponent *data, DataPropStr prop) {
  const char *out = NULL;
  switch (prop) {
  case DATA_PROP_COLOR: out = get_x_prop_value(data, "X-ERRANDS-COLOR", "none"); break;
  case DATA_PROP_LIST_NAME: out = get_x_prop_value(data, "X-ERRANDS-LIST-NAME", ""); break;
  case DATA_PROP_LIST_UID: out = get_x_prop_value(data, "X-ERRANDS-LIST-UID", ""); break;
  case DATA_PROP_NOTES: out = icalcomponent_get_description(data); break;
  case DATA_PROP_PARENT: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_RELATEDTO_PROPERTY);
    if (property) out = icalproperty_get_relatedto(property);
    break;
  }
  case DATA_PROP_RRULE: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_RRULE_PROPERTY);
    if (property) {
      struct icalrecurrencetype rrule = icalproperty_get_rrule(property);
      out = icalrecurrencetype_as_string(&rrule);
    }
    break;
  }
  case DATA_PROP_TEXT: out = icalcomponent_get_summary(data); break;
  case DATA_PROP_UID: out = icalcomponent_get_uid(data); break;
  }
  return out;
}

size_t errands_data_get_int(icalcomponent *data, DataPropInt prop) {
  switch (prop) {
  case DATA_PROP_PERCENT: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_PERCENTCOMPLETE_PROPERTY);
    return property ? icalproperty_get_percentcomplete(property) : 0;
  }
  case DATA_PROP_PRIORITY: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_PRIORITY_PROPERTY);
    return property ? icalproperty_get_priority(property) : 0;
  }
  }
  return 0;
}

bool errands_data_get_bool(icalcomponent *data, DataPropBool prop) {
  bool out = false;
  switch (prop) {
  case DATA_PROP_DELETED: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-DELETED", "0")); break;
  case DATA_PROP_EXPANDED: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-EXPANDED", "0")); break;
  case DATA_PROP_NOTIFIED: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-NOTIFIED", "0")); break;
  case DATA_PROP_TOOLBAR_SHOWN: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-TOOLBAR-SHOWN", "0")); break;
  case DATA_PROP_TRASH: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-TRASH", "0")); break;
  case DATA_PROP_SYNCED: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-SYNCED", "0")); break;
  }
  return out;
}

GStrv errands_data_get_strv(icalcomponent *data, DataPropStrv prop) {
  GStrv out = NULL;
  switch (prop) {
  case DATA_PROP_ATTACHMENTS: {
    const char *property = get_x_prop_value(data, "X-ERRANDS-ATTACHMENTS", NULL);
    if (property) out = g_strsplit(property, ",", -1);
    break;
  }
  case DATA_PROP_TAGS: {
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    for (icalproperty *p = icalcomponent_get_first_property(data, ICAL_CATEGORIES_PROPERTY); p != 0;
         p = icalcomponent_get_next_property(data, ICAL_CATEGORIES_PROPERTY))
      g_strv_builder_add(builder, icalproperty_get_value_as_string(p));
    out = g_strv_builder_end(builder);
    break;
  }
  }
  return out;
}

icaltimetype errands_data_get_time(icalcomponent *data, DataPropTime prop) {
  icaltimetype out = {0};
  switch (prop) {
  case DATA_PROP_CHANGED_TIME: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_LASTMODIFIED_PROPERTY);
    if (property) {
      out = icalproperty_get_lastmodified(property);
    } else {
      property = icalcomponent_get_first_property(data, ICAL_DTSTAMP_PROPERTY);
      if (property) out = icalproperty_get_dtstamp(property);
    }
    break;
  }
  case DATA_PROP_COMPLETED_TIME: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_COMPLETED_PROPERTY);
    if (property) out = icalproperty_get_completed(property);
    break;
  }
  case DATA_PROP_CREATED_TIME: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_CREATED_PROPERTY);
    if (property) {
      out = icalproperty_get_created(property);
    } else {
      property = icalcomponent_get_first_property(data, ICAL_DTSTAMP_PROPERTY);
      if (property) out = icalproperty_get_dtstamp(property);
    }
    break;
  }
  case DATA_PROP_DUE_TIME: out = icalcomponent_get_due(data); break;
  case DATA_PROP_END_TIME: out = icalcomponent_get_dtend(data); break;
  case DATA_PROP_START_TIME: out = icalcomponent_get_dtstart(data); break;
  }
  if (icaltime_is_null_time(out)) out.is_date = true;
  return out;
}

// --- SETTERS --- //

void errands_data_set_str(icalcomponent *data, DataPropStr prop, const char *value) {
  switch (prop) {
  case DATA_PROP_COLOR: set_x_prop_value(data, "X-ERRANDS-COLOR", value); break;
  case DATA_PROP_LIST_NAME: set_x_prop_value(data, "X-ERRANDS-LIST-NAME", value); break;
  case DATA_PROP_LIST_UID: set_x_prop_value(data, "X-ERRANDS-LIST-UID", value); break;
  case DATA_PROP_NOTES: {
    if (!value || !strcmp(value, "")) {
      icalcomponent_remove_property(data, icalcomponent_get_first_property(data, ICAL_RELATEDTO_PROPERTY));
      break;
    }
    icalcomponent_set_description(data, value);
    break;
  }
  case DATA_PROP_PARENT: {
    if (!value || !strcmp(value, "")) {
      icalcomponent_remove_property(data, icalcomponent_get_first_property(data, ICAL_RELATEDTO_PROPERTY));
      break;
    }
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_RELATEDTO_PROPERTY);
    if (property) icalproperty_set_relatedto(property, value);
    else icalcomponent_add_property(data, icalproperty_new_relatedto(value));
    break;
  }
  case DATA_PROP_RRULE: {
    if (!value || !strcmp(value, "")) {
      icalcomponent_remove_property(data, icalcomponent_get_first_property(data, ICAL_RRULE_PROPERTY));
      break;
    }
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_RRULE_PROPERTY);
    if (property) icalproperty_set_rrule(property, icalrecurrencetype_from_string(value));
    else icalcomponent_add_property(data, icalproperty_new_rrule(icalrecurrencetype_from_string(value)));
    break;
  }
  case DATA_PROP_TEXT: {
    if (!value || !strcmp(value, "")) {
      icalcomponent_remove_property(data, icalcomponent_get_first_property(data, ICAL_SUMMARY_PROPERTY));
      break;
    }
    icalcomponent_set_summary(data, value);
    break;
  }
  case DATA_PROP_UID: icalcomponent_set_uid(data, value); break;
  }
  if (icalcomponent_isa(data) == ICAL_VTODO_COMPONENT)
    errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
}

void errands_data_set_int(icalcomponent *data, DataPropInt prop, size_t value) {
  switch (prop) {
  case DATA_PROP_PERCENT: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_PERCENTCOMPLETE_PROPERTY);
    if (property) icalproperty_set_percentcomplete(property, value);
    else icalcomponent_add_property(data, icalproperty_new_percentcomplete(value));
    break;
  }
  case DATA_PROP_PRIORITY: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_PRIORITY_PROPERTY);
    if (property) icalproperty_set_priority(property, value);
    else icalcomponent_add_property(data, icalproperty_new_priority(value));
    break;
  }
  }
  errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
}

void errands_data_set_bool(icalcomponent *data, DataPropBool prop, bool value) {
  const char *str = value ? "1" : "0";
  switch (prop) {
  case DATA_PROP_DELETED: set_x_prop_value(data, "X-ERRANDS-DELETED", str); break;
  case DATA_PROP_EXPANDED: set_x_prop_value(data, "X-ERRANDS-EXPANDED", str); break;
  case DATA_PROP_NOTIFIED: set_x_prop_value(data, "X-ERRANDS-NOTIFIED", str); break;
  case DATA_PROP_TOOLBAR_SHOWN: set_x_prop_value(data, "X-ERRANDS-TOOLBAR-SHOWN", str); break;
  case DATA_PROP_TRASH: set_x_prop_value(data, "X-ERRANDS-TRASH", str); break;
  case DATA_PROP_SYNCED: set_x_prop_value(data, "X-ERRANDS-SYNCED", str); break;
  }
  if (icalcomponent_isa(data) == ICAL_VTODO_COMPONENT)
    errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
}

void errands_data_set_strv(icalcomponent *data, DataPropStrv prop, GStrv value) {
  g_autofree gchar *str = g_strjoinv(",", value);
  switch (prop) {
  case DATA_PROP_ATTACHMENTS: set_x_prop_value(data, "X-ERRANDS-ATTACHMENTS", str); break;
  case DATA_PROP_TAGS:
    for (icalproperty *p = icalcomponent_get_first_property(data, ICAL_CATEGORIES_PROPERTY); p != 0;
         p = icalcomponent_get_next_property(data, ICAL_CATEGORIES_PROPERTY))
      icalcomponent_remove_property(data, p);
    for (size_t i = 0; i < g_strv_length(value); i++)
      icalcomponent_add_property(data, icalproperty_new_categories(value[i]));
    break;
  }
  errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
}

void errands_data_set_time(icalcomponent *data, DataPropTime prop, icaltimetype value) {
  switch (prop) {
  case DATA_PROP_CHANGED_TIME: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_LASTMODIFIED_PROPERTY);
    if (icaltime_is_null_date(value)) icalcomponent_remove_property(data, property);
    else {
      if (!property) {
        property = icalproperty_new_lastmodified(value);
        icalcomponent_add_property(data, property);
      } else icalproperty_set_lastmodified(property, value);
    }
    break;
  }
  case DATA_PROP_COMPLETED_TIME: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_COMPLETED_PROPERTY);
    if (icaltime_is_null_date(value)) icalcomponent_remove_property(data, property);
    else {
      if (!property) {
        property = icalproperty_new_completed(value);
        icalcomponent_add_property(data, property);
      } else icalproperty_set_completed(property, value);
    }
    errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
    break;
  }
  case DATA_PROP_CREATED_TIME: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_CREATED_PROPERTY);
    if (icaltime_is_null_date(value)) icalcomponent_remove_property(data, property);
    else {
      if (!property) {
        property = icalproperty_new_created(value);
        icalcomponent_add_property(data, property);
      } else icalproperty_set_created(property, value);
    }
    errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
    break;
  }
  case DATA_PROP_DUE_TIME: {
    if (icaltime_is_null_date(value)) {
      icalproperty *property = icalcomponent_get_first_property(data, ICAL_DUE_PROPERTY);
      icalcomponent_remove_property(data, property);
    } else icalcomponent_set_due(data, value);
    errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
    break;
  }
  case DATA_PROP_END_TIME: {
    if (icaltime_is_null_date(value)) {
      icalproperty *property = icalcomponent_get_first_property(data, ICAL_DTEND_PROPERTY);
      icalcomponent_remove_property(data, property);
    } else icalcomponent_set_dtend(data, value);
    errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
    break;
  }
  case DATA_PROP_START_TIME: {
    if (icaltime_is_null_date(value)) {
      icalproperty *property = icalcomponent_get_first_property(data, ICAL_DTSTART_PROPERTY);
      icalcomponent_remove_property(data, property);
    } else icalcomponent_set_dtstart(data, value);
    errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
    break;
  }
  }
}

void errands_data_add_tag(icalcomponent *data, DataPropStrv prop, const char *tag) {
  switch (prop) {
  case DATA_PROP_TAGS: icalcomponent_add_property(data, icalproperty_new_categories(tag)); break;
  case DATA_PROP_ATTACHMENTS: break;
  }
  errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
}

void errands_data_remove_tag(icalcomponent *data, DataPropStrv prop, const char *tag) {
  switch (prop) {
  case DATA_PROP_TAGS: {
    for (icalproperty *p = icalcomponent_get_first_property(data, ICAL_CATEGORIES_PROPERTY); p;
         p = icalcomponent_get_next_property(data, ICAL_CATEGORIES_PROPERTY)) {
      if (STR_EQUAL(tag, icalproperty_get_value_as_string(p))) icalcomponent_remove_property(data, p);
    }
  } break;
  case DATA_PROP_ATTACHMENTS: break;
  }
  errands_data_set_time(data, DATA_PROP_CHANGED_TIME, icaltime_get_date_time_now());
}
