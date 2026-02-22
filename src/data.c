#include "data.h"
#include "settings.h"

#include "vendor/json.h"
#include "vendor/toolbox.h"

AUTOPTR_DEFINE(JSON, json_free)

// ---------- GLOBALS ---------- //

GPtrArray *errands_data_lists;
static gchar *user_dir, *calendars_dir, *backups_dir;

// ---------- PRIVATE FUNCTIONS ---------- //

static void errands_data_create_backup() {
  g_mkdir_with_parents(backups_dir, 0755);
  // Count files in backups_dir
  autofree char *out = NULL;
  int res = cmd_run_stdout(tmp_str_printf("ls %s | wc -l", backups_dir), &out);
  if (res != 0 && !out) return;
  // Remove oldest backup
  if (STR_TO_UL(out) >= 20) {
    LOG("User Data: Removing oldest backup");
    system(tmp_str_printf("rm -f $(find %s/* -type f | sort | head -n 1)", backups_dir));
  }
  // Create backup
  time_t t = TIME_NOW;
  struct tm *tm = localtime(&t);
  char time_str[15];
  strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", tm);
  res = system(tmp_str_printf("cd %s && tar -cJf backups/%s.tar.xz calendars", user_dir, time_str));
  LOG("User Data: %s backup at %s", res == 0 ? "Created" : "Failed to create",
      tmp_str_printf("%s/%s.tar.xz", backups_dir, time_str));
}

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
        errands_list_data_create(list_uid_item->string_val, name_item->string_val, NULL, color_item->string_val,
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
           attachment_item = attachment_item->next)
        g_strv_builder_add(builder, attachment_item->string_val);
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
      JSON *notes_item = json_object_get(task_item, "notes");
      JSON *notified_item = json_object_get(task_item, "notified");
      JSON *parent_item = json_object_get(task_item, "parent");
      JSON *percent_complete_item = json_object_get(task_item, "percent_complete");
      JSON *priority_item = json_object_get(task_item, "priority");
      JSON *rrule_item = json_object_get(task_item, "rrule");
      JSON *start_date_item = json_object_get(task_item, "start_date");
      JSON *text_item = json_object_get(task_item, "text");
      JSON *uid_item = json_object_get(task_item, "uid");
      // Create iCalendar event
      icalcomponent *ical = icalcomponent_new(ICAL_VTODO_COMPONENT);
      if (completed_item) errands_data_set_completed(ical, icaltime_get_date_time_now());
      if (tags) errands_data_set_tags(ical, tags);
      if (changed_at_item) errands_data_set_changed(ical, icaltime_from_string(changed_at_item->string_val));
      if (created_at_item) errands_data_set_created(ical, icaltime_from_string(created_at_item->string_val));
      if (due_date_item) errands_data_set_due(ical, icaltime_from_string(due_date_item->string_val));
      if (notes_item) errands_data_set_notes(ical, notes_item->string_val);
      if (parent_item) errands_data_set_parent(ical, parent_item->string_val);
      if (percent_complete_item) errands_data_set_percent(ical, percent_complete_item->int_val);
      if (priority_item) errands_data_set_priority(ical, priority_item->int_val);
      if (rrule_item) errands_data_set_rrule(ical, icalrecurrencetype_from_string(rrule_item->string_val));
      if (start_date_item) errands_data_set_start(ical, icaltime_from_string(start_date_item->string_val));
      if (text_item) errands_data_set_text(ical, text_item->string_val);
      if (uid_item) errands_data_set_uid(ical, uid_item->string_val);
      errands_data_set_attachments(ical, attachments);
      errands_data_set_color(ical, color_item->string_val, false);
      errands_data_set_deleted(ical, deleted_item->bool_val);
      errands_data_set_notified(ical, notified_item->bool_val);
      icalcomponent_add_component(calendar->ical, ical);
    }
    // Save calendar to file
    const char *calendar_filename = tmp_str_printf("%s.ics", list_uid_item->string_val);
    g_autofree gchar *calendar_file_path = g_build_filename(calendars_dir, calendar_filename, NULL);
    bool res = write_string_to_file(calendar_file_path, icalcomponent_as_ical_string(calendar->ical));
    if (!res) LOG("User Data: Failed to save calendar to %s", calendar_file_path);
  }
  remove(old_data_file);
}

static void collect_and_sort_children_recursive(TaskData *parent, GPtrArray *all_tasks) {
  const char *uid = errands_data_get_uid(parent->ical);
  for_range(i, 0, all_tasks->len) {
    icalcomponent *ical = g_ptr_array_index(all_tasks, i);
    const char *parent_uid = errands_data_get_parent(ical);
    if (parent_uid && STR_EQUAL(uid, parent_uid)) {
      TaskData *new_task = errands_task_data_new(ical, parent, parent->list);
      collect_and_sort_children_recursive(new_task, all_tasks);
    }
  }
  g_ptr_array_sort_values(parent->children, errands_data_sort_func);
}

static void errands__list_data_save_cb(ListData *data) {
  if (!data) return;
  const char *path = tmp_str_printf("%s/%s.ics", calendars_dir, data->uid);
  const char *ical = icalcomponent_as_ical_string(data->ical);
  if (ical && !g_file_set_contents(path, ical, -1, NULL)) {
    LOG("User Data: Failed to save list '%s'", path);
    return;
  }
  LOG("User Data: Saved list '%s'", path);
  // bool need_remove = false;
  // if (errands_data_get_deleted(data->ical)) {
  //   if (errands_settings_get(SETTING_SYNC).b) {
  //     if (errands_data_get_synced(data->ical)) need_remove = true;
  //   } else need_remove = true;
  // }
  // if (need_remove) {
  //   LOG("Data: Remove: %s", path);
  //   g_ptr_array_remove(errands_data_lists, data);
  //   data = NULL;
  //   remove(path);
  // }
}

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
  if (!val && property) icalcomponent_remove_property(ical, property);
  else icalproperty_set_x(property, val);
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_data_init() {
  errands_data_lists = g_ptr_array_new_with_free_func((GDestroyNotify)errands_list_data_free);
  user_dir = g_build_filename(g_get_user_data_dir(), "errands", NULL);
  calendars_dir = g_build_filename(user_dir, "calendars", NULL);
  backups_dir = g_build_filename(user_dir, "backups", NULL);

  g_mkdir_with_parents(calendars_dir, 0755);
  errands_data_migrate_from_46();

  errands_data_create_backup();

  // --- Load lists --- //

  g_autoptr(GDir) dir = g_dir_open(calendars_dir, 0, NULL);
  if (!dir) return;
  const char *filename;
  TIMER_START;
  while ((filename = g_dir_read_name(dir))) {
    if (!g_str_has_suffix(filename, ".ics")) continue;
    g_autofree gchar *path = g_build_filename(calendars_dir, filename, NULL);
    g_autofree gchar *content = read_file_to_string(path);
    if (!content) continue;
    icalcomponent *cal = icalparser_parse_string(content);
    if (!cal) continue;
    // Delete file if calendar deleted
    if (errands_data_get_deleted(cal)) {
      if ((errands_settings_get(SETTING_SYNC).b && errands_data_get_synced(cal)) ||
          !errands_settings_get(SETTING_SYNC).b) {
        LOG("User Data: Calendar was deleted. Removing %s", path);
        icalcomponent_free(cal);
        remove(path);
        continue;
      }
    }
    const char *uid = path_file_name(filename);
    ListData *list_data = errands_list_data_load_from_ical(cal, uid, NULL, NULL);
    CONTINUE_IF(!list_data);
    g_ptr_array_add(errands_data_lists, list_data);
    LOG("User Data: Loaded calendar %s", path);
  }

  LOG("User Data: Loaded %d task-lists in %f sec.", errands_data_lists->len, TIMER_ELAPSED_MS);
}

void errands_data_cleanup(void) {
  LOG("User Data: Cleanup");
  if (user_dir) g_free(user_dir);
  if (calendars_dir) g_free(calendars_dir);
  if (backups_dir) g_free(backups_dir);
  if (errands_data_lists) g_ptr_array_free(errands_data_lists, true);
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

ListData *errands_data_find_list_data_by_uid(const char *uid) {
  for_range(i, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, i);
    if (strcmp(list->uid, uid) == 0) return list;
  }
  return NULL;
}

// ---------- LIST DATA ---------- //

ListData *errands_list_data_new(icalcomponent *ical, const char *uid) {
  ListData *data = calloc(1, sizeof(ListData));
  data->ical = ical;
  data->children = g_ptr_array_new_with_free_func((GDestroyNotify)errands_task_data_free);
  data->uid = strdup(uid);

  return data;
}

ListData *errands_list_data_load_from_ical(icalcomponent *ical, const char *uid, const char *name, const char *color) {
  if (!ical || !uid) return NULL;
  if (ical && icalcomponent_isa(ical) != ICAL_VCALENDAR_COMPONENT) return NULL;
  get_x_prop_value(ical, "X-WR-CALNAME", name ? name : uid);
  get_x_prop_value(ical, "X-APPLE-CALENDAR-COLOR", color ? color : generate_hex_as_str());
  icalcomponent_strip_errors(ical);

  ListData *list_data = errands_list_data_new(ical, uid);

  // Collect all tasks and add toplevel tasks to lists
  g_autoptr(GPtrArray) all_tasks = g_ptr_array_new();
  for (icalcomponent *c = icalcomponent_get_first_component(ical, ICAL_VTODO_COMPONENT); c != 0;
       c = icalcomponent_get_next_component(ical, ICAL_VTODO_COMPONENT)) {
    CONTINUE_IF(errands_data_get_deleted(c)); // TODO: check sync
    g_ptr_array_add(all_tasks, c);
    CONTINUE_IF(errands_data_get_parent(c));
    errands_task_data_new(c, NULL, list_data);
  }
  g_ptr_array_sort_values(list_data->children, errands_data_sort_func);
  // Collect children recursively
  for_range(i, 0, list_data->children->len) {
    TaskData *toplevel_task = g_ptr_array_index(list_data->children, i);
    collect_and_sort_children_recursive(toplevel_task, all_tasks);
  }

  return list_data;
}

// TODO: why we need synced?
ListData *errands_list_data_create(const char *uid, const char *name, const char *description, const char *color,
                                   bool deleted, bool synced) {
  ASSERT(uid != NULL && name != NULL && color != NULL);

  icalcomponent *ical = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
  icalcomponent_add_property(ical, icalproperty_new_version("2.0"));
  icalcomponent_add_property(ical, icalproperty_new_prodid("~//Errands"));
  set_x_prop_value(ical, "X-ERRANDS-DELETED", BOOL_TO_STR_NUM(deleted));
  set_x_prop_value(ical, "X-ERRANDS-SYNCED", BOOL_TO_STR_NUM(synced));
  set_x_prop_value(ical, "X-WR-CALNAME", name);
  set_x_prop_value(ical, "X-WR-CALDESC", description);
  set_x_prop_value(ical, "X-APPLE-CALENDAR-COLOR", color);

  return errands_list_data_new(ical, uid);
}

static void errands_list_data_sort_recursive(GPtrArray *array) {
  g_ptr_array_sort_values(array, errands_data_sort_func);
  for_range(i, 0, array->len) {
    TaskData *task_data = g_ptr_array_index(array, i);
    errands_list_data_sort_recursive(task_data->children);
  }
}

void errands_list_data_sort_toplevel(ListData *data) {
  g_ptr_array_sort_values(data->children, errands_data_sort_func);
}

void errands_list_data_sort(ListData *data) {
  errands_list_data_sort_toplevel(data);
  for_range(i, 0, data->children->len) {
    TaskData *task_data = g_ptr_array_index(data->children, i);
    errands_list_data_sort_recursive(task_data->children);
  }
}

GPtrArray *errands_list_data_get_all_tasks_as_icalcomponents(ListData *data) {
  if (icalcomponent_isa(data->ical) != ICAL_VCALENDAR_COMPONENT) return NULL;
  GPtrArray *tasks = g_ptr_array_new();
  for (icalcomponent *c = icalcomponent_get_first_component(data->ical, ICAL_VTODO_COMPONENT); c != 0;
       c = icalcomponent_get_next_component(data->ical, ICAL_VTODO_COMPONENT))
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

void errands_list_data_save(ListData *data) { g_idle_add_once((GSourceOnceFunc)errands__list_data_save_cb, data); }

void errands_list_data_free(ListData *data) {
  if (!data) return;
  if (data->ical) icalcomponent_free(data->ical);
  if (data->uid) free(data->uid);
  if (data->children) g_ptr_array_free(data->children, true);
  free(data);
}

// ---------- TASK DATA ---------- //

TaskData *errands_task_data_new(icalcomponent *ical, TaskData *parent, ListData *list) {
  TaskData *task = calloc(1, sizeof(TaskData));
  task->ical = ical;
  task->parent = parent;
  task->list = list;
  task->children = g_ptr_array_new_with_free_func((GDestroyNotify)errands_task_data_free);
  if (errands_data_get_parent(ical) && parent) {
    g_ptr_array_add(parent->children, task);
  } else {
    if (list) g_ptr_array_add(list->children, task);
  }

  return task;
}

TaskData *errands_task_data_create_task(ListData *list, TaskData *parent, const char *text) {

  TaskData *task = errands_task_data_new(icalcomponent_new(ICAL_VTODO_COMPONENT), parent, list);
  errands_data_set_uid(task->ical, generate_uuid4());
  errands_data_set_text(task->ical, text);
  if (parent) errands_data_set_parent(task->ical, errands_data_get_uid(parent->ical));
  errands_data_set_created(task->ical, icaltime_get_date_time_now());
  if (list) icalcomponent_add_component(list->ical, task->ical);

  return task;
}

void errands_task_data_sort_sub_tasks(TaskData *data) {
  g_ptr_array_sort_values(data->children, errands_data_sort_func);
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

void errands_task_data_print(TaskData *data) {
  LOG("[%s] %s", errands_data_is_completed(data->ical) ? "x" : " ", errands_data_get_uid(data->ical));
}

void errands_task_data_get_flat_list(TaskData *parent, GPtrArray *array) {
  for_range(i, 0, parent->children->len) {
    TaskData *sub_task = g_ptr_array_index(parent->children, i);
    g_ptr_array_add(array, sub_task);
    errands_task_data_get_flat_list(sub_task, array);
  }
}

TaskData *errands_task_data_move_to_list(TaskData *data, ListData *list, TaskData *parent) {
  if (!data || !list) return NULL;
  GPtrArray *arr_to_remove_from = data->parent ? data->parent->children : data->list->children;
  icalcomponent *clone = icalcomponent_new_clone(data->ical);
  icalcomponent_remove_component(data->list->ical, data->ical);
  icalcomponent_add_component(list->ical, clone);
  TaskData *new_data = errands_task_data_new(clone, parent, list);
  errands_data_set_uid(new_data->ical, generate_uuid4());
  errands_data_set_parent(new_data->ical, parent ? errands_data_get_parent(parent->ical) : NULL);
  for_range(i, 0, data->children->len)
      errands_task_data_move_to_list(g_ptr_array_index(data->children, i), list, new_data);
  g_ptr_array_remove(arr_to_remove_from, data);
  return new_data;
}

TaskData *errands_task_data_find_by_uid(ListData *list, const char *uid) {
  g_autoptr(GPtrArray) tasks = g_ptr_array_sized_new(list->children->len);
  errands_list_data_get_flat_list(list, tasks);
  for_range(i, 0, tasks->len) {
    TaskData *task = g_ptr_array_index(tasks, i);
    if (STR_EQUAL(errands_data_get_uid(task->ical), uid)) return task;
  }
  return NULL;
}

void errands_task_data_free(TaskData *data) {
  if (!data) return;
  if (data->children) g_ptr_array_free(data->children, true);
  free(data);
}

// ---------- PRINTING ---------- //

// gchar *list_data_print(icalcomponent* data) {
//   const char *name = errands_data_get_prop(data, PROP_LIST_NAME);
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

// void task_data_print(icalcomponent* data, GString *out, size_t indent) {
//   const uint8_t max_line_len = 72;
//   for (size_t i = 0; i < indent; i++) g_string_append(out, "  ");
//   g_string_append_printf(out, "[%s] ",
//                          !icaltime_is_null_time(errands_data_get_prop(data, PROP_COMPLETED_TIME)) ? "x" : " ");
//   const char *text = errands_data_get_prop(data, PROP_TEXT);
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

// ---------- PROPERTIES ---------- //

// --- BOOL --- //

bool errands_data_get_cancelled(icalcomponent *ical) {
  icalproperty_status status = icalcomponent_get_status(ical);
  return status == ICAL_STATUS_CANCELLED;
}
bool errands_data_get_deleted(icalcomponent *ical) {
  return STR_TO_BOOL(get_x_prop_value(ical, "X-ERRANDS-DELETED", "0"));
}
bool errands_data_get_notified(icalcomponent *ical) {
  return STR_TO_BOOL(get_x_prop_value(ical, "X-ERRANDS-NOTIFIED", "0"));
}
bool errands_data_get_synced(icalcomponent *ical) {
  return STR_TO_BOOL(get_x_prop_value(ical, "X-ERRANDS-SYNCED", "0"));
}

bool errands_data_is_completed(icalcomponent *ical) { return !icaltime_is_null_time(errands_data_get_completed(ical)); }
bool errands_data_is_due(icalcomponent *ical) {
  icaltimetype due_time = errands_data_get_due(ical);
  if (icaltime_is_null_time(due_time)) return false;
  bool is_due = false;
  if (due_time.is_date) is_due = icaltime_compare_date_only(due_time, icaltime_today()) < 1;
  else is_due = icaltime_compare(due_time, icaltime_get_date_time_now()) < 1;
  return is_due;
}

void errands_data_set_cancelled(icalcomponent *ical, bool value) {
  icalcomponent_set_status(ical, value ? ICAL_STATUS_CANCELLED : ICAL_STATUS_NEEDSACTION);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_deleted(icalcomponent *ical, bool value) {
  set_x_prop_value(ical, "X-ERRANDS-DELETED", BOOL_TO_STR_NUM(value));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_notified(icalcomponent *ical, bool value) {
  set_x_prop_value(ical, "X-ERRANDS-NOTIFIED", BOOL_TO_STR_NUM(value));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_synced(icalcomponent *ical, bool value) {
  set_x_prop_value(ical, "X-ERRANDS-SYNCED", BOOL_TO_STR_NUM(value));
}

// --- INT --- //

int errands_data_get_percent(icalcomponent *ical) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_PERCENTCOMPLETE_PROPERTY);
  return property ? icalproperty_get_percentcomplete(property) : 0;
}
int errands_data_get_priority(icalcomponent *ical) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_PRIORITY_PROPERTY);
  return property ? icalproperty_get_priority(property) : 0;
}

void errands_data_set_percent(icalcomponent *ical, int value) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_PERCENTCOMPLETE_PROPERTY);
  if (property) icalproperty_set_percentcomplete(property, value);
  else icalcomponent_add_property(ical, icalproperty_new_percentcomplete(value));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_priority(icalcomponent *ical, int value) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_PRIORITY_PROPERTY);
  if (property) icalproperty_set_percentcomplete(property, value);
  else icalcomponent_add_property(ical, icalproperty_new_priority(value));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}

// --- STRING --- //

const char *errands_data_get_color(icalcomponent *ical, bool list) {
  if (list) return get_x_prop_value(ical, "X-APPLE-CALENDAR-COLOR", NULL);
  else {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_COLOR_PROPERTY);
    return property ? icalproperty_get_color(property) : NULL;
  }
}
const char *errands_data_get_list_name(icalcomponent *ical) {
  return get_x_prop_value(ical, "X-WR-CALNAME", "Untitled");
}
const char *errands_data_get_list_description(icalcomponent *ical) {
  return get_x_prop_value(ical, "X-WR-CALDESC", NULL);
}
const char *errands_data_get_notes(icalcomponent *ical) { return icalcomponent_get_description(ical); }
const char *errands_data_get_parent(icalcomponent *ical) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_RELATEDTO_PROPERTY);
  return property ? icalproperty_get_relatedto(property) : NULL;
}
const char *errands_data_get_text(icalcomponent *ical) { return icalcomponent_get_summary(ical); }
const char *errands_data_get_uid(icalcomponent *ical) { return icalcomponent_get_uid(ical); }

void errands_data_set_notes(icalcomponent *ical, const char *value) {
  if (!value || STR_EQUAL(value, ""))
    icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_DESCRIPTION_PROPERTY));
  else icalcomponent_set_description(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_color(icalcomponent *ical, const char *value, bool list) {
  if (list) {
    set_x_prop_value(ical, "X-APPLE-CALENDAR-COLOR", value);
  } else {
    if (!value || STR_EQUAL(value, ""))
      icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_COLOR_PROPERTY));
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_COLOR_PROPERTY);
    if (property) icalproperty_set_color(property, value);
    else icalcomponent_add_property(ical, icalproperty_new_color(value));
    errands_data_set_changed(ical, icaltime_get_date_time_now());
  }
  errands_data_set_synced(ical, false);
}
void errands_data_set_list_name(icalcomponent *ical, const char *value) {
  set_x_prop_value(ical, "X-WR-CALNAME", value);
  errands_data_set_synced(ical, false);
}
void errands_data_set_list_description(icalcomponent *ical, const char *value) {
  set_x_prop_value(ical, "X-WR-CALDESC", value);
  errands_data_set_synced(ical, false);
}
void errands_data_set_parent(icalcomponent *ical, const char *value) {
  if (!value || STR_EQUAL(value, ""))
    icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_RELATEDTO_PROPERTY));
  else {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_RELATEDTO_PROPERTY);
    if (property) icalproperty_set_relatedto(property, value);
    else icalcomponent_add_property(ical, icalproperty_new_relatedto(value));
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_text(icalcomponent *ical, const char *value) {
  if (!value || STR_EQUAL(value, ""))
    icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_SUMMARY_PROPERTY));
  else icalcomponent_set_summary(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_uid(icalcomponent *ical, const char *value) {
  if (!value || STR_EQUAL(value, ""))
    icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_UID_PROPERTY));
  else icalcomponent_set_uid(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}

// --- RRULE --- //

struct icalrecurrencetype errands_data_get_rrule(icalcomponent *ical) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_RRULE_PROPERTY);
  return property ? icalproperty_get_rrule(property) : (struct icalrecurrencetype)ICALRECURRENCETYPE_INITIALIZER;
}
void errands_data_set_rrule(icalcomponent *ical, struct icalrecurrencetype value) {
  struct icalrecurrencetype null = ICALRECURRENCETYPE_INITIALIZER;
  if (icalrecurrencetype_compare(&value, &null))
    icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_RRULE_PROPERTY));
  else {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_RRULE_PROPERTY);
    if (property) icalproperty_set_rrule(property, value);
    else icalcomponent_add_property(ical, property);
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}

// --- STRV --- //

GStrv errands_data_get_attachments(icalcomponent *ical) {
  const char *property = get_x_prop_value(ical, "X-ERRANDS-ATTACHMENTS", NULL);
  return property ? g_strsplit(property, ",", -1) : NULL;
}
GStrv errands_data_get_tags(icalcomponent *ical) {
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for (icalproperty *p = icalcomponent_get_first_property(ical, ICAL_CATEGORIES_PROPERTY); p != 0;
       p = icalcomponent_get_next_property(ical, ICAL_CATEGORIES_PROPERTY))
    g_strv_builder_add(builder, icalproperty_get_value_as_string(p));
  return g_strv_builder_end(builder);
}

void errands_data_set_attachments(icalcomponent *ical, GStrv value) {
  g_autofree gchar *str = g_strjoinv(",", value);
  set_x_prop_value(ical, "X-ERRANDS-ATTACHMENTS", str);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_tags(icalcomponent *ical, GStrv value) {
  for (icalproperty *p = icalcomponent_get_first_property(ical, ICAL_CATEGORIES_PROPERTY); p != 0;
       p = icalcomponent_get_next_property(ical, ICAL_CATEGORIES_PROPERTY))
    icalcomponent_remove_property(ical, p);
  for (size_t i = 0; i < g_strv_length(value); i++)
    icalcomponent_add_property(ical, icalproperty_new_categories(value[i]));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}

void errands_data_add_tag(icalcomponent *ical, const char *tag) {
  icalcomponent_add_property(ical, icalproperty_new_categories(tag));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
bool errands_data_remove_tag(icalcomponent *ical, const char *tag) {
  for (icalproperty *p = icalcomponent_get_first_property(ical, ICAL_CATEGORIES_PROPERTY); p;
       p = icalcomponent_get_next_property(ical, ICAL_CATEGORIES_PROPERTY)) {
    if (STR_EQUAL(tag, icalproperty_get_value_as_string(p))) {
      icalcomponent_remove_property(ical, p);
      errands_data_set_synced(ical, false);
      errands_data_set_changed(ical, icaltime_get_date_time_now());
      return true;
    }
  }
  return false;
}

// --- TIME --- //

icaltimetype errands_data_get_changed(icalcomponent *ical) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_LASTMODIFIED_PROPERTY);
  if (property) return icalproperty_get_lastmodified(property);
  else {
    property = icalcomponent_get_first_property(ical, ICAL_DTSTAMP_PROPERTY);
    if (property) return icalproperty_get_dtstamp(property);
  }
  errands_data_set_changed(ical, icaltime_get_date_time_now());
  return icalproperty_get_lastmodified(icalcomponent_get_first_property(ical, ICAL_LASTMODIFIED_PROPERTY));
}
icaltimetype errands_data_get_completed(icalcomponent *ical) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_COMPLETED_PROPERTY);
  if (property) return icalproperty_get_completed(property);
  return icaltime_null_time();
}
icaltimetype errands_data_get_created(icalcomponent *ical) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_CREATED_PROPERTY);
  if (property) return icalproperty_get_created(property);
  else {
    property = icalcomponent_get_first_property(ical, ICAL_DTSTAMP_PROPERTY);
    if (property) return icalproperty_get_dtstamp(property);
  }
  errands_data_set_created(ical, icaltime_get_date_time_now());
  return icalproperty_get_created(icalcomponent_get_first_property(ical, ICAL_CREATED_PROPERTY));
}
icaltimetype errands_data_get_due(icalcomponent *ical) { return icalcomponent_get_due(ical); }
icaltimetype errands_data_get_end(icalcomponent *ical) { return icalcomponent_get_dtend(ical); }
icaltimetype errands_data_get_start(icalcomponent *ical) { return icalcomponent_get_dtstart(ical); }

void errands_data_set_changed(icalcomponent *ical, icaltimetype value) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_LASTMODIFIED_PROPERTY);
  if (icaltime_is_null_time(value) && property) icalcomponent_remove_property(ical, property);
  else {
    if (!property) icalcomponent_add_property(ical, icalproperty_new_lastmodified(value));
    else icalproperty_set_lastmodified(property, value);
  }
}
void errands_data_set_completed(icalcomponent *ical, icaltimetype value) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_COMPLETED_PROPERTY);
  if (icaltime_is_null_time(value) && property) {
    icalcomponent_remove_property(ical, property);
    errands_data_set_percent(ical, 0);
    icalcomponent_set_status(ical, ICAL_STATUS_NEEDSACTION);
  } else {
    if (!property) icalcomponent_add_property(ical, icalproperty_new_completed(value));
    else icalproperty_set_completed(property, value);
    errands_data_set_percent(ical, 100);
    icalcomponent_set_status(ical, ICAL_STATUS_COMPLETED);
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_created(icalcomponent *ical, icaltimetype value) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_CREATED_PROPERTY);
  if (icaltime_is_null_time(value) && property) icalcomponent_remove_property(ical, property);
  else {
    if (!property) icalcomponent_add_property(ical, icalproperty_new_created(value));
    else icalproperty_set_created(property, value);
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_due(icalcomponent *ical, icaltimetype value) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_DUE_PROPERTY);
  if (icaltime_is_null_time(value) && property) icalcomponent_remove_property(ical, property);
  else icalcomponent_set_due(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_end(icalcomponent *ical, icaltimetype value) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_DTEND_PROPERTY);
  if (icaltime_is_null_time(value) && property) icalcomponent_remove_property(ical, property);
  else icalcomponent_set_dtend(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_start(icalcomponent *ical, icaltimetype value) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_DTSTART_PROPERTY);
  if (icaltime_is_null_time(value) && property) icalcomponent_remove_property(ical, property);
  else icalcomponent_set_dtstart(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}

// ---------- SORT AND FILTER FUNCTIONS ---------- //

gint errands_data_sort_func(gconstpointer a, gconstpointer b) {
  if (!a || !b) return 0;
  TaskData *td_a = (TaskData *)a;
  TaskData *td_b = (TaskData *)b;

  // Cancelled sort
  gboolean cancelled_a = errands_data_get_cancelled(td_a->ical);
  gboolean cancelled_b = errands_data_get_cancelled(td_b->ical);
  if (cancelled_a != cancelled_b) return cancelled_a - cancelled_b;

  // Completion sort
  gboolean completed_a = !icaltime_is_null_date(errands_data_get_completed(td_a->ical));
  gboolean completed_b = !icaltime_is_null_date(errands_data_get_completed(td_b->ical));
  if (completed_a != completed_b) return completed_a - completed_b;

  // Then apply global sort
  bool asc_order = errands_settings_get(SETTING_SORT_ORDER).i;
  switch (errands_settings_get(SETTING_SORT_BY).i) {
  case SORT_TYPE_CREATION_DATE: {
    icaltimetype creation_date_a = errands_data_get_created(asc_order ? td_b->ical : td_a->ical);
    icaltimetype creation_date_b = errands_data_get_created(asc_order ? td_a->ical : td_b->ical);
    return icaltime_compare(creation_date_b, creation_date_a);
  }
  case SORT_TYPE_DUE_DATE: {
    icaltimetype due_a = errands_data_get_due(asc_order ? td_b->ical : td_a->ical);
    icaltimetype due_b = errands_data_get_due(asc_order ? td_a->ical : td_b->ical);
    bool null_a = icaltime_is_null_time(due_a);
    bool null_b = icaltime_is_null_time(due_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(due_a, due_b);
  }
  case SORT_TYPE_PRIORITY: {
    int p_a = errands_data_get_priority(asc_order ? td_b->ical : td_a->ical);
    int p_b = errands_data_get_priority(asc_order ? td_a->ical : td_b->ical);
    return p_b - p_a;
  }
  case SORT_TYPE_START_DATE: {
    icaltimetype start_a = errands_data_get_start(asc_order ? td_b->ical : td_a->ical);
    icaltimetype start_b = errands_data_get_start(asc_order ? td_a->ical : td_b->ical);
    bool null_a = icaltime_is_null_time(start_a);
    bool null_b = icaltime_is_null_time(start_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(start_a, start_b);
  }
  default: return 0;
  }
}

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
