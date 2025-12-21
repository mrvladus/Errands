#include "data.h"
#include "settings.h"

#include "vendor/json.h"
#include "vendor/toolbox.h"

static void errands_data_create_backup();

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
  if (STR_TO_UL(out) > 19) {
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

    autoptr(ErrandsData) calendar =
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
      JSON *uid_item = json_object_get(task_item, "uid");
      // Create iCalendar event
      ErrandsData event = {0};
      event.ical = icalcomponent_new(ICAL_VTODO_COMPONENT);
      if (completed_item) {
        icaltimetype time = icaltime_get_date_time_now();
        errands_data_set_prop(&event, PROP_COMPLETED_TIME, &time);
      }
      if (tags) errands_data_set_prop(&event, PROP_TAGS, tags);
      if (changed_at_item) {
        icaltimetype time = icaltime_from_string(changed_at_item->string_val);
        errands_data_set_prop(&event, PROP_CHANGED_TIME, &time);
      }
      if (created_at_item) {
        icaltimetype time = icaltime_from_string(created_at_item->string_val);
        errands_data_set_prop(&event, PROP_CREATED_TIME, &time);
      }
      if (due_date_item) {
        icaltimetype time = icaltime_from_string(due_date_item->string_val);
        errands_data_set_prop(&event, PROP_DUE_TIME, &time);
      }
      if (notes_item) errands_data_set_prop(&event, PROP_NOTES, notes_item->string_val);
      if (parent_item) errands_data_set_prop(&event, PROP_PARENT, parent_item->string_val);
      if (percent_complete_item) errands_data_set_prop(&event, PROP_PERCENT, &percent_complete_item->int_val);
      if (priority_item) errands_data_set_prop(&event, PROP_PRIORITY, &priority_item->int_val);
      if (rrule_item) errands_data_set_prop(&event, PROP_RRULE, rrule_item->string_val);
      if (start_date_item) {
        icaltimetype time = icaltime_from_string(start_date_item->string_val);
        errands_data_set_prop(&event, PROP_START_TIME, &time);
      }
      if (text_item) errands_data_set_prop(&event, PROP_TEXT, text_item->string_val);
      if (uid_item) errands_data_set_prop(&event, PROP_UID, uid_item->string_val);
      errands_data_set_prop(&event, PROP_ATTACHMENTS, attachments);
      errands_data_set_prop(&event, PROP_COLOR, color_item->string_val);
      errands_data_set_prop(&event, PROP_DELETED, &deleted_item->bool_val);
      errands_data_set_prop(&event, PROP_EXPANDED, &expanded_item->bool_val);
      errands_data_set_prop(&event, PROP_NOTIFIED, &notified_item->bool_val);
      icalcomponent_add_component(calendar->ical, event.ical);
    }
    // Save calendar to file
    const char *calendar_filename = tmp_str_printf("%s.ics", list_uid_item->string_val);
    g_autofree gchar *calendar_file_path = g_build_filename(calendars_dir, calendar_filename, NULL);
    bool res = write_string_to_file(calendar_file_path, icalcomponent_as_ical_string(calendar->ical));
    if (!res) LOG("User Data: Failed to save calendar to %s", calendar_file_path);
  }
  remove(old_data_file);
}

static void collect_and_sort_children_recursive(ErrandsData *parent, GPtrArray *all_tasks) {
  const char *uid = errands_data_get_prop(parent, PROP_UID).s;
  for_range(i, 0, all_tasks->len) {
    ErrandsData *task = g_ptr_array_index(all_tasks, i);
    const char *parent_uid = errands_data_get_prop(task, PROP_PARENT).s;
    if (parent_uid && STR_EQUAL(uid, parent_uid)) {
      ErrandsData *child = errands_task_data_new(task->ical, parent, parent->as.task.list);
      collect_and_sort_children_recursive(child, all_tasks);
    }
  }
  g_ptr_array_sort_values(parent->children, errands_data_sort_func);
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
  icalproperty_set_x(property, val);
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_data_init() {
  errands_data_lists = g_ptr_array_new_with_free_func((GDestroyNotify)errands_data_free);
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
    if (errands_data_get_prop(&ERRANDS_DATA_ICAL_WRAPPER(cal), PROP_DELETED).b) {
      if ((errands_settings_get(SETTING_SYNC).b &&
           errands_data_get_prop(&ERRANDS_DATA_ICAL_WRAPPER(cal), PROP_SYNCED).b) ||
          !errands_settings_get(SETTING_SYNC).b) {
        LOG("User Data: Calendar was deleted. Removing %s", path);
        icalcomponent_free(cal);
        remove(path);
        continue;
      }
    }
    // TODO: pass filename as uid
    g_ptr_array_add(errands_data_lists, errands_list_data_load_from_ical(cal, NULL, NULL, NULL));
    LOG("User Data: Loaded calendar %s", path);
  }

  // --- Load tasks --- //

  // Collect all tasks and add toplevel tasks to lists
  g_autoptr(GPtrArray) all_tasks = g_ptr_array_new();
  for_range(i, 0, errands_data_lists->len) {
    ErrandsData *list_data = g_ptr_array_index(errands_data_lists, i);
    for (icalcomponent *c = icalcomponent_get_first_component(list_data->ical, ICAL_VTODO_COMPONENT); c != 0;
         c = icalcomponent_get_next_component(list_data->ical, ICAL_VTODO_COMPONENT)) {
      if (errands_data_get_prop(&ERRANDS_DATA_ICAL_WRAPPER(c), PROP_DELETED).b) continue;
      ErrandsData *task_data = errands_task_data_new_from_ical(c, NULL, list_data);
      g_ptr_array_add(all_tasks, task_data);
    }
    g_ptr_array_sort_values(list_data->children, errands_data_sort_func);
  }

  // Collect children recursively
  for_range(i, 0, errands_data_lists->len) {
    ErrandsData *list_data = g_ptr_array_index(errands_data_lists, i);
    for_range(j, 0, list_data->children->len) {
      ErrandsData *toplevel_task = g_ptr_array_index(list_data->children, j);
      collect_and_sort_children_recursive(toplevel_task, all_tasks);
    }
  }

  LOG("User Data: Loaded %d tasks from %d lists in %f sec.", all_tasks->len, errands_data_lists->len, TIMER_ELAPSED_MS);
}

void errands_data_cleanup(void) {
  LOG("User Data: Cleanup");
  g_free(user_dir);
  g_free(calendars_dir);
  g_free(backups_dir);
  if (errands_data_lists) g_ptr_array_free(errands_data_lists, true);
}

void errands_data_get_flat_list(GPtrArray *tasks) {
  for_range(i, 0, errands_data_lists->len) {
    ErrandsData *list = g_ptr_array_index(errands_data_lists, i);
    errands_list_data_get_flat_list(list, tasks);
  }
}

void errands_data_sort() {
  for_range(i, 0, errands_data_lists->len) {
    ErrandsData *list = g_ptr_array_index(errands_data_lists, i);
    errands_list_data_sort(list);
  }
}

// ---------- LIST DATA ---------- //

ErrandsData *errands_list_data_new(icalcomponent *ical, const char *uid, const char *name, const char *color) {
  ErrandsData *data = calloc(1, sizeof(ErrandsData));
  data->type = ERRANDS_DATA_TYPE_TASK;
  data->ical = ical;
  data->as.list.uid = strdup(uid);
  data->as.list.name = strdup(name);
  data->as.list.color = strdup(color);

  return data;
}

ErrandsData *errands_list_data_load_from_ical(icalcomponent *ical, const char *uid, const char *name,
                                              const char *color) {
  if (ical && icalcomponent_isa(ical) != ICAL_VCALENDAR_COMPONENT) return NULL;
  const char *_uid = NULL;
  const char *_name = NULL;
  const char *_color = NULL;
  if (ical && icalcomponent_isa(ical) == ICAL_VCALENDAR_COMPONENT) {
    _uid = icalcomponent_get_uid(ical);
    if (!_uid) _uid = generate_uuid4();
    _name = get_x_prop_value(ical, "X-ERRANDS-LIST-NAME", _uid);
    _color = get_x_prop_value(ical, "X-ERRANDS-COLOR", generate_hex_as_str());
  };

  ErrandsData *list_data = errands_list_data_new(ical, _uid, _name, _color);

  // Collect all tasks and add toplevel tasks to lists
  g_autoptr(GPtrArray) all_tasks = g_ptr_array_new();
  for (icalcomponent *c = icalcomponent_get_first_component(ical, ICAL_VTODO_COMPONENT); c != 0;
       c = icalcomponent_get_next_component(ical, ICAL_VTODO_COMPONENT)) {
    if (errands_data_get_prop(&ERRANDS_DATA_ICAL_WRAPPER(c), PROP_DELETED).b) continue;
    ErrandsData *task_data = errands_task_data_new(c, NULL, list_data);
    g_ptr_array_add(all_tasks, task_data);
    if (errands_data_get_prop(task_data, PROP_PARENT).s) continue;
  }
  g_ptr_array_sort_values(list_data->children, errands_data_sort_func);
  // Collect children recursively
  for_range(i, 0, list_data->children->len) {
    ErrandsData *toplevel_task = g_ptr_array_index(list_data->children, i);
    collect_and_sort_children_recursive(toplevel_task, all_tasks);
  }

  return list_data;
}

ErrandsData *errands_list_data_create(const char *uid, const char *name, const char *color, bool deleted, bool synced) {
  if (!name || !uid || !color) return NULL;

  icalcomponent *ical = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
  icalcomponent_add_property(ical, icalproperty_new_version("2.0"));
  icalcomponent_add_property(ical, icalproperty_new_prodid("~//Errands"));
  set_x_prop_value(ical, "X-ERRANDS-DELETED", BOOL_TO_STR_NUM(deleted));
  set_x_prop_value(ical, "X-ERRANDS-SYNCED", BOOL_TO_STR_NUM(synced));
  ErrandsData *list_data = errands_list_data_new(ical, uid, name, color);

  return list_data;
}

static void errands_list_data_sort_recursive(GPtrArray *array) {
  g_ptr_array_sort_values(array, errands_data_sort_func);
  for_range(i, 0, array->len) {
    ErrandsData *task_data = g_ptr_array_index(array, i);
    errands_list_data_sort_recursive(task_data->children);
  }
}

void errands_list_data_sort_toplevel(ErrandsData *data) {
  g_ptr_array_sort_values(data->children, errands_data_sort_func);
}

void errands_list_data_sort(ErrandsData *data) {
  errands_list_data_sort_toplevel(data);
  for_range(i, 0, data->children->len) {
    ErrandsData *task_data = g_ptr_array_index(data->children, i);
    errands_list_data_sort_recursive(task_data->children);
  }
}

GPtrArray *errands_list_data_get_all_tasks_as_icalcomponents(ErrandsData *data) {
  if (icalcomponent_isa(data->ical) != ICAL_VCALENDAR_COMPONENT) return NULL;
  GPtrArray *tasks = g_ptr_array_new();
  for (icalcomponent *c = icalcomponent_get_first_component(data->ical, ICAL_VTODO_COMPONENT); c != 0;
       c = icalcomponent_get_next_component(data->ical, ICAL_VTODO_COMPONENT))
    g_ptr_array_add(tasks, c);

  return tasks;
}

void errands_list_data_print(ErrandsData *data) {
  for_range(i, 0, data->children->len) {
    ErrandsData *task_data = g_ptr_array_index(data->children, i);
    errands_task_data_print(task_data);
  }
}

void errands_list_data_get_flat_list(ErrandsData *data, GPtrArray *tasks) {
  for_range(i, 0, data->children->len) {
    ErrandsData *task_data = g_ptr_array_index(data->children, i);
    g_ptr_array_add(tasks, task_data);
    errands_task_data_get_flat_list(task_data, tasks);
  }
}

// ---------- TASK DATA ---------- //

ErrandsData *errands_task_data_new(icalcomponent *ical, ErrandsData *parent, ErrandsData *list) {
  if (!list) return NULL;

  ErrandsData *task = calloc(1, sizeof(ErrandsData));
  task->type = ERRANDS_DATA_TYPE_TASK;
  task->ical = ical;
  task->as.task.parent = parent;
  task->as.task.list = list;
  task->children = g_ptr_array_new_with_free_func((GDestroyNotify)errands_data_free);
  if (errands_data_get_prop(task, PROP_PARENT).s && parent) g_ptr_array_add(parent->children, task);
  else g_ptr_array_add(list->children, task);

  return task;
}

ErrandsData *errands_task_data_create_task(ErrandsData *list, ErrandsData *parent, const char *text) {
  if (!list || !text) return NULL;

  ErrandsData *task = errands_task_data_new(icalcomponent_new(ICAL_VTODO_COMPONENT), parent, list);
  errands_data_set_prop(task, PROP_UID, (void *)generate_uuid4());
  errands_data_set_prop(task, PROP_TEXT, (void *)text);
  if (parent) {
    const char *parent_uid = errands_data_get_prop(parent, PROP_UID).s;
    errands_data_set_prop(task, PROP_PARENT, (void *)parent_uid);
  }
  icaltimetype created = icaltime_get_date_time_now();
  errands_data_set_prop(task, PROP_CREATED_TIME, &created);
  icalcomponent_add_component(list->ical, task->ical);

  return task;
}

void errands_task_data_sort_sub_tasks(ErrandsData *data) {
  g_ptr_array_sort_values(data->children, errands_data_sort_func);
}

size_t errands_task_data_get_indent_level(ErrandsData *data) {
  if (!data) return 0;
  size_t indent = 0;
  ErrandsData *parent = data->as.task.parent;
  while (parent) {
    indent++;
    parent = parent->as.task.parent;
  }
  return indent;
}

void errands_task_data_print(ErrandsData *data) {
  const char *text = errands_data_get_prop(data, PROP_TEXT).s;
  LOG("[%s] %s", errands_task_data_is_completed(data) ? "x" : " ", text);
}

void errands_task_data_get_flat_list(ErrandsData *parent, GPtrArray *array) {
  for_range(i, 0, parent->children->len) {
    ErrandsData *sub_task = g_ptr_array_index(parent->children, i);
    g_ptr_array_add(array, sub_task);
    errands_task_data_get_flat_list(sub_task, array);
  }
}

bool errands_task_data_is_due(ErrandsData *data) {
  icaltimetype due_time = errands_data_get_prop(data, PROP_DUE_TIME).t;
  if (icaltime_is_null_time(due_time)) return false;
  bool is_due = false;
  if (due_time.is_date) is_due = icaltime_compare_date_only(due_time, icaltime_today()) < 1;
  else is_due = icaltime_compare(due_time, icaltime_get_date_time_now()) < 1;
  return is_due;
}

bool errands_task_data_is_completed(ErrandsData *data) {
  return !icaltime_is_null_date(errands_data_get_prop(data, PROP_COMPLETED_TIME).t);
}

ErrandsData *errands_task_data_move_to_list(ErrandsData *data, ErrandsData *list, ErrandsData *parent) {
  if (!data || !list) return NULL;
  GPtrArray *arr_to_remove_from = data->as.task.parent ? data->as.task.parent->children : data->as.task.list->children;
  icalcomponent *clone = icalcomponent_new_clone(data->ical);
  icalcomponent_remove_component(data->as.task.list->ical, data->ical);
  icalcomponent_add_component(list->ical, clone);
  ErrandsData *new_data = errands_task_data_new(clone, parent, list);
  errands_data_set_prop(new_data, PROP_UID, (void *)generate_uuid4());
  errands_data_set_prop(new_data, PROP_PARENT, parent ? (void *)errands_data_get_prop(parent, PROP_UID).s : NULL);
  for_range(i, 0, data->children->len)
      errands_task_data_move_to_list(g_ptr_array_index(data->children, i), list, new_data);
  g_ptr_array_remove(arr_to_remove_from, data);
  return new_data;
}

// ---------- PRINTING ---------- //

// gchar *list_data_print(ListData *data) {
//   const char *name = errands_data_get_str(data, PROP_LIST_NAME);
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

// void task_data_print(TaskData *data, GString *out, size_t indent) {
//   const uint8_t max_line_len = 72;
//   for (size_t i = 0; i < indent; i++) g_string_append(out, "  ");
//   g_string_append_printf(out, "[%s] ",
//                          !icaltime_is_null_time(errands_data_get_prop(data, PROP_COMPLETED_TIME)) ? "x" : " ");
//   const char *text = errands_data_get_str(data, PROP_TEXT);
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

static void errands__list_data_save_cb(ErrandsData *data) {
  const char *path = tmp_str_printf("%s/%s.ics", calendars_dir, data->as.list.uid);
  if (!g_file_set_contents(path, icalcomponent_as_ical_string(data->ical), -1, NULL)) {
    LOG("User Data: Failed to save list '%s'", path);
    return;
  }
  LOG("User Data: Saved list '%s'", path);
}

void errands_list_data_save(ErrandsData *data) {
  if (!data || data->type != ERRANDS_DATA_TYPE_LIST) return;
  g_idle_add_once((GSourceOnceFunc)errands__list_data_save_cb, data);
}

void errands_data_free(ErrandsData *data) {
  if (!data) return;
  if (data->type == ERRANDS_DATA_TYPE_LIST) {
    if (data->ical) icalcomponent_free(data->ical);
    if (data->as.list.uid) free(data->as.list.uid);
    if (data->as.list.color) free(data->as.list.color);
    if (data->as.list.name) free(data->as.list.name);
  }
  if (data->children) g_ptr_array_free(data->children, true);
  free(data);
}

// ---------- PROPERTIES FUNCTIONS ---------- //

ErrandsPropRes errands_data_get_prop(const ErrandsData *data, ErrandsProp prop) {
  ErrandsPropRes res = {0};
  icalcomponent *ical = data->ical;

  switch (prop) {
  case PROP_CANCELLED: res.b = (bool)atoi(get_x_prop_value(ical, "X-ERRANDS-CANCELLED", "0")); break;
  case PROP_DELETED: res.b = (bool)atoi(get_x_prop_value(ical, "X-ERRANDS-DELETED", "0")); break;
  case PROP_EXPANDED: res.b = (bool)atoi(get_x_prop_value(ical, "X-ERRANDS-EXPANDED", "0")); break;
  case PROP_NOTIFIED: res.b = (bool)atoi(get_x_prop_value(ical, "X-ERRANDS-NOTIFIED", "0")); break;
  case PROP_PINNED: res.b = (bool)atoi(get_x_prop_value(ical, "X-ERRANDS-PINNED", "0")); break;
  case PROP_SYNCED: res.b = (bool)atoi(get_x_prop_value(ical, "X-ERRANDS-SYNCED", "0")); break;

  case PROP_PERCENT: {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_PERCENTCOMPLETE_PROPERTY);
    res.i = property ? icalproperty_get_percentcomplete(property) : 0;
  } break;
  case PROP_PRIORITY: {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_PRIORITY_PROPERTY);
    res.i = property ? icalproperty_get_priority(property) : 0;
  } break;

  case PROP_COLOR: res.s = get_x_prop_value(ical, "X-ERRANDS-COLOR", "none"); break;
  case PROP_LIST_NAME: res.s = get_x_prop_value(ical, "X-ERRANDS-LIST-NAME", ""); break;
  case PROP_NOTES: res.s = icalcomponent_get_description(ical); break;
  case PROP_PARENT: {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_RELATEDTO_PROPERTY);
    res.s = property ? icalproperty_get_relatedto(property) : NULL;
  } break;
  case PROP_RRULE: {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_RRULE_PROPERTY);
    if (property) {
      struct icalrecurrencetype rrule = icalproperty_get_rrule(property);
      res.s = icalrecurrencetype_as_string(&rrule);
    }
  } break;
  case PROP_TEXT: res.s = icalcomponent_get_summary(ical); break;
  case PROP_UID: res.s = icalcomponent_get_uid(ical); break;

  case PROP_ATTACHMENTS: {
    const char *property = get_x_prop_value(ical, "X-ERRANDS-ATTACHMENTS", NULL);
    res.sv = property ? g_strsplit(property, ",", -1) : NULL;
  } break;
  case PROP_TAGS: {
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    for (icalproperty *p = icalcomponent_get_first_property(ical, ICAL_CATEGORIES_PROPERTY); p != 0;
         p = icalcomponent_get_next_property(ical, ICAL_CATEGORIES_PROPERTY))
      g_strv_builder_add(builder, icalproperty_get_value_as_string(p));
    res.sv = g_strv_builder_end(builder);
  } break;

  case PROP_CHANGED_TIME: {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_LASTMODIFIED_PROPERTY);
    if (property) res.t = icalproperty_get_lastmodified(property);
    else {
      property = icalcomponent_get_first_property(ical, ICAL_DTSTAMP_PROPERTY);
      if (property) res.t = icalproperty_get_dtstamp(property);
    }
  } break;
  case PROP_COMPLETED_TIME: {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_COMPLETED_PROPERTY);
    if (property) res.t = icalproperty_get_completed(property);
  } break;
  case PROP_CREATED_TIME: {
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_CREATED_PROPERTY);
    if (property) res.t = icalproperty_get_created(property);
    else {
      property = icalcomponent_get_first_property(ical, ICAL_DTSTAMP_PROPERTY);
      if (property) res.t = icalproperty_get_dtstamp(property);
    }
  } break;
  case PROP_DUE_TIME: res.t = icalcomponent_get_due(ical); break;
  case PROP_END_TIME: res.t = icalcomponent_get_dtend(ical); break;
  case PROP_START_TIME: res.t = icalcomponent_get_dtstart(ical); break;
  }

  return res;
}

#define UPDATE_TIMESTAMP                                                                                               \
  if (data->type == ERRANDS_DATA_TYPE_TASK) {                                                                          \
    icaltimetype __now__ = icaltime_get_date_time_now();                                                               \
    errands_data_set_prop(data, PROP_CHANGED_TIME, &__now__);                                                          \
  }

void errands_data_set_prop(ErrandsData *data, ErrandsProp prop, void *value) {
#define BREAK_IF_NOT_TASK                                                                                              \
  if (data->type != ERRANDS_DATA_TYPE_TASK) break
#define BREAK_IF_NOT_LIST                                                                                              \
  if (data->type != ERRANDS_DATA_TYPE_LIST) break

  icalcomponent *ical = data->ical;

  bool bool_value = *(bool *)value;
  int int_value = *(int *)value;
  const char *str_value = value;
  icaltimetype time_value = *(icaltimetype *)value;
  GStrv strv_value = (GStrv)value;
  g_autofree gchar *strv_value_str = g_strjoinv(",", (gchar **)value);

  icalproperty *property = NULL;

  switch (prop) {
  case PROP_CANCELLED: {
    BREAK_IF_NOT_TASK;
    set_x_prop_value(ical, "X-ERRANDS-CANCELLED", BOOL_TO_STR_NUM(bool_value));
    UPDATE_TIMESTAMP;
  } break;
  case PROP_DELETED: {
    set_x_prop_value(ical, "X-ERRANDS-DELETED", BOOL_TO_STR_NUM(bool_value));
    UPDATE_TIMESTAMP;
  } break;
  case PROP_EXPANDED: {
    BREAK_IF_NOT_TASK;
    set_x_prop_value(ical, "X-ERRANDS-EXPANDED", BOOL_TO_STR_NUM(bool_value));
    UPDATE_TIMESTAMP;
  } break;
  case PROP_NOTIFIED: {
    BREAK_IF_NOT_TASK;
    set_x_prop_value(ical, "X-ERRANDS-NOTIFIED", BOOL_TO_STR_NUM(bool_value));
    UPDATE_TIMESTAMP;
  } break;
  case PROP_PINNED: {
    BREAK_IF_NOT_TASK;
    set_x_prop_value(ical, "X-ERRANDS-PINNED", BOOL_TO_STR_NUM(bool_value));
    UPDATE_TIMESTAMP;
  } break;
  case PROP_SYNCED: {
    set_x_prop_value(ical, "X-ERRANDS-SYNCED", BOOL_TO_STR_NUM(bool_value));
    UPDATE_TIMESTAMP;
  } break;

  case PROP_PERCENT: {
    BREAK_IF_NOT_TASK;
    property = icalcomponent_get_first_property(ical, ICAL_PERCENTCOMPLETE_PROPERTY);
    if (property) icalproperty_set_percentcomplete(property, int_value);
    else icalcomponent_add_property(ical, icalproperty_new_percentcomplete(int_value));
    UPDATE_TIMESTAMP;
  } break;
  case PROP_PRIORITY: {
    BREAK_IF_NOT_TASK;
    property = icalcomponent_get_first_property(ical, ICAL_PRIORITY_PROPERTY);
    if (property) icalproperty_set_priority(property, int_value);
    else icalcomponent_add_property(ical, icalproperty_new_priority(int_value));
    UPDATE_TIMESTAMP;
  } break;

  case PROP_COLOR: {
    set_x_prop_value(ical, "X-ERRANDS-COLOR", value);
    UPDATE_TIMESTAMP;
  } break;
  case PROP_LIST_NAME: {
    BREAK_IF_NOT_LIST;
    set_x_prop_value(ical, "X-ERRANDS-LIST-NAME", value);
    UPDATE_TIMESTAMP;
  } break;
  case PROP_NOTES: {
    BREAK_IF_NOT_TASK;
    if (!str_value || STR_EQUAL(str_value, ""))
      icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_DESCRIPTION_PROPERTY));
    else icalcomponent_set_description(ical, str_value);
    UPDATE_TIMESTAMP;
  } break;
  case PROP_PARENT: {
    BREAK_IF_NOT_TASK;
    if (!str_value || STR_EQUAL(str_value, ""))
      icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_RELATEDTO_PROPERTY));
    else {
      property = icalcomponent_get_first_property(ical, ICAL_RELATEDTO_PROPERTY);
      if (property) icalproperty_set_relatedto(property, value);
      else icalcomponent_add_property(ical, icalproperty_new_relatedto(value));
    }
    UPDATE_TIMESTAMP;
  } break;
  case PROP_RRULE: {
    BREAK_IF_NOT_TASK;
    if (!str_value || STR_EQUAL(str_value, ""))
      icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_RRULE_PROPERTY));
    else {
      property = icalcomponent_get_first_property(ical, ICAL_RRULE_PROPERTY);
      if (property) icalproperty_set_rrule(property, icalrecurrencetype_from_string(str_value));
      else icalcomponent_add_property(ical, icalproperty_new_rrule(icalrecurrencetype_from_string(str_value)));
    }
    UPDATE_TIMESTAMP;
  } break;
  case PROP_TEXT: {
    BREAK_IF_NOT_TASK;
    if (!str_value || STR_EQUAL(str_value, ""))
      icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_SUMMARY_PROPERTY));
    else icalcomponent_set_summary(ical, value);
    UPDATE_TIMESTAMP;
  } break;
  case PROP_UID: {
    icalcomponent_set_uid(ical, value);
    UPDATE_TIMESTAMP;
  } break;

  case PROP_ATTACHMENTS: {
    BREAK_IF_NOT_TASK;
    set_x_prop_value(ical, "X-ERRANDS-ATTACHMENTS", strv_value_str);
    UPDATE_TIMESTAMP;
  } break;
  case PROP_TAGS: {
    for (icalproperty *p = icalcomponent_get_first_property(ical, ICAL_CATEGORIES_PROPERTY); p != 0;
         p = icalcomponent_get_next_property(ical, ICAL_CATEGORIES_PROPERTY))
      icalcomponent_remove_property(ical, p);
    for (size_t i = 0; i < g_strv_length(strv_value); i++)
      icalcomponent_add_property(ical, icalproperty_new_categories(strv_value[i]));
  } break;

  case PROP_CHANGED_TIME: {
    property = icalcomponent_get_first_property(ical, ICAL_LASTMODIFIED_PROPERTY);
    if (icaltime_is_null_date(time_value) && property) icalcomponent_remove_property(ical, property);
    else {
      if (!property) icalcomponent_add_property(ical, icalproperty_new_lastmodified(time_value));
      else icalproperty_set_lastmodified(property, time_value);
    }
  } break;
  case PROP_COMPLETED_TIME: {
    property = icalcomponent_get_first_property(ical, ICAL_COMPLETED_PROPERTY);
    if (icaltime_is_null_date(time_value) && property) icalcomponent_remove_property(ical, property);
    else {
      if (!property) icalcomponent_add_property(ical, icalproperty_new_completed(time_value));
      else icalproperty_set_completed(property, time_value);
    }
    UPDATE_TIMESTAMP;
  } break;
  case PROP_CREATED_TIME: {
    property = icalcomponent_get_first_property(ical, ICAL_CREATED_PROPERTY);
    if (icaltime_is_null_date(time_value) && property) icalcomponent_remove_property(ical, property);
    else {
      if (!property) icalcomponent_add_property(ical, icalproperty_new_created(time_value));
      else icalproperty_set_created(property, time_value);
    }
    UPDATE_TIMESTAMP;
  } break;
  case PROP_DUE_TIME: {
    property = icalcomponent_get_first_property(ical, ICAL_DUE_PROPERTY);
    if (icaltime_is_null_date(time_value) && property) icalcomponent_remove_property(ical, property);
    else icalcomponent_set_due(ical, time_value);
    UPDATE_TIMESTAMP;
  } break;
  case PROP_END_TIME: {
    property = icalcomponent_get_first_property(ical, ICAL_DTEND_PROPERTY);
    if (icaltime_is_null_date(time_value) && property) icalcomponent_remove_property(ical, property);
    else icalcomponent_set_dtend(ical, time_value);
    UPDATE_TIMESTAMP;
  } break;
  case PROP_START_TIME: {
    property = icalcomponent_get_first_property(ical, ICAL_DTSTART_PROPERTY);
    if (icaltime_is_null_date(time_value) && property) icalcomponent_remove_property(ical, property);
    else icalcomponent_set_dtstart(ical, time_value);
    UPDATE_TIMESTAMP;
  } break;
  }
}

void errands_data_add_tag(ErrandsData *data, const char *tag) {
  icalcomponent_add_property(data->ical, icalproperty_new_categories(tag));
  UPDATE_TIMESTAMP;
}

void errands_data_remove_tag(ErrandsData *data, const char *tag) {
  for (icalproperty *p = icalcomponent_get_first_property(data->ical, ICAL_CATEGORIES_PROPERTY); p;
       p = icalcomponent_get_next_property(data->ical, ICAL_CATEGORIES_PROPERTY)) {
    if (STR_EQUAL(tag, icalproperty_get_value_as_string(p))) icalcomponent_remove_property(data->ical, p);
  }
  UPDATE_TIMESTAMP;
}

// ---------- SORT AND FILTER FUNCTIONS ---------- //

gint errands_data_sort_func(gconstpointer a, gconstpointer b) {
  if (!a || !b) return 0;
  ErrandsData *td_a = (ErrandsData *)a;
  ErrandsData *td_b = (ErrandsData *)b;

  // Completion sort first
  gboolean completed_a = !icaltime_is_null_date(errands_data_get_prop(td_a, PROP_COMPLETED_TIME).t);
  gboolean completed_b = !icaltime_is_null_date(errands_data_get_prop(td_b, PROP_COMPLETED_TIME).t);
  if (completed_a != completed_b) return completed_a - completed_b;

  // Then apply global sort
  bool asc_order = errands_settings_get(SETTING_SORT_ORDER).i;
  switch (errands_settings_get(SETTING_SORT_BY).i) {
  case SORT_TYPE_CREATION_DATE: {
    icaltimetype creation_date_a = errands_data_get_prop(asc_order ? td_b : td_a, PROP_CREATED_TIME).t;
    icaltimetype creation_date_b = errands_data_get_prop(asc_order ? td_a : td_b, PROP_CREATED_TIME).t;
    return icaltime_compare(creation_date_b, creation_date_a);
  }
  case SORT_TYPE_DUE_DATE: {
    icaltimetype due_a = errands_data_get_prop(asc_order ? td_b : td_a, PROP_DUE_TIME).t;
    icaltimetype due_b = errands_data_get_prop(asc_order ? td_a : td_b, PROP_DUE_TIME).t;
    bool null_a = icaltime_is_null_time(due_a);
    bool null_b = icaltime_is_null_time(due_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(due_a, due_b);
  }
  case SORT_TYPE_PRIORITY: {
    int p_a = errands_data_get_prop(asc_order ? td_b : td_a, PROP_PRIORITY).i;
    int p_b = errands_data_get_prop(asc_order ? td_a : td_b, PROP_PRIORITY).i;
    return p_b - p_a;
  }
  case SORT_TYPE_START_DATE: {
    icaltimetype start_a = errands_data_get_prop(asc_order ? td_b : td_a, PROP_START_TIME).t;
    icaltimetype start_b = errands_data_get_prop(asc_order ? td_a : td_b, PROP_START_TIME).t;
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
