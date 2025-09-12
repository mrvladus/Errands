#include "../settings.h"
#include "../utils.h"
#include "data.h"

#include "../vendor/json.h"
#include "../vendor/toolbox.h"
#include "glib.h"

#include <gio/gio.h>
#include <libical/ical.h>

static tb_ptr_array lists_to_write = {0};

static const char *user_dir;

static void errands_data_migrate_from_46() {
  g_autofree gchar *old_data_file = g_build_filename(user_dir, "data.json", NULL);
  if (!g_file_test(old_data_file, G_FILE_TEST_EXISTS)) return;
  tb_log("User Data: Migrate");
  g_autoptr(GError) error = NULL;
  gsize length;
  g_autofree gchar *contents = NULL;
  // Read file contents
  if (!g_file_get_contents(old_data_file, &contents, &length, &error)) {
    tb_log("User Data: Failed to read old data file: %s", error->message);
    return;
  }
  // Parse JSON
  JSON *root = json_parse(contents);
  if (!root) return;
  // Process lists
  JSON *cal_arr = json_object_get(root, "lists");
  JSON *cal_item = NULL;
  for (JSON *cal_item = cal_arr->child; cal_item; cal_item = cal_item->next) {
    JSON *color_item = json_object_get(cal_item, "color");
    JSON *deleted_item = json_object_get(cal_item, "deleted");
    JSON *synced_item = json_object_get(cal_item, "synced");
    JSON *name_item = json_object_get(cal_item, "name");
    JSON *list_uid_item = json_object_get(cal_item, "uid");

    icalcomponent *calendar = list_data_new(list_uid_item->string_val, name_item->string_val, color_item->string_val,
                                            deleted_item->bool_val, synced_item->bool_val, 0);

    // Process tasks
    JSON *tasks_arr = json_object_get(root, "tasks");
    if (tasks_arr->type != JSON_TYPE_ARRAY) continue;
    for (JSON *task_item = tasks_arr->child; task_item; task_item = task_item->next) {
      JSON *task_list_uid_item = json_object_get(task_item, "list_uid");
      const gchar *task_list_uid = task_list_uid_item ? task_list_uid_item->string_val : NULL;
      if (!task_list_uid || !g_str_equal(task_list_uid, list_uid_item->string_val)) continue;
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
      for (JSON *tags_item = task_tags_arr->child; tags_item; tags_item = tags_item->next)
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
      icalcomponent_add_component(calendar, event);
    }
    // Save calendar to file
    const char *calendar_filename = tb_tmp_str_printf("%s.ics", list_uid_item->string_val);
    g_autofree gchar *calendar_file_path = g_build_filename(user_dir, calendar_filename, NULL);
    if (!g_file_set_contents(calendar_file_path, icalcomponent_as_ical_string(calendar), -1, &error))
      tb_log("User Data: Failed to save calendar to %s: %s", calendar_file_path, error->message);
  }
  // Clean up
  json_free(root);
  remove(old_data_file);
}

static void write_lists() {
  for (size_t i = 0; i < lists_to_write.size; ++i) {
    icalcomponent *list_data = lists_to_write.items[i];
    const char *filename = tb_tmp_str_printf("%s.ics", errands_data_get_str(list_data, DATA_PROP_LIST_UID));
    g_autofree gchar *path = g_build_filename(user_dir, filename, NULL);
    if (!g_file_set_contents(path, icalcomponent_as_ical_string(list_data), -1, NULL)) {
      tb_log("User Data: Failed to save list '%s'", path);
      return;
    }
    tb_log("User Data: Saved list '%s'", path);
  }
  tb_ptr_array_reset(&lists_to_write);
}

static void write_list(ListData *list_data) {
  if (!list_data) return;
  g_autofree gchar *path = g_strdup_printf("%s/%s.ics", user_dir, errands_data_get_str(list_data, DATA_PROP_LIST_UID));
  if (!g_file_set_contents(path, icalcomponent_as_ical_string(list_data), -1, NULL)) {
    tb_log("User Data: Failed to save list '%s'", path);
    return;
  }
  tb_log("User Data: Saved list '%s'", path);
}

void errands_data_load_lists() {
  user_dir = g_build_filename(g_get_user_data_dir(), "errands", NULL);
  if (!tb_dir_exists(user_dir)) {
    tb_log("User Data: Creating user data directory at %s", user_dir);
    g_mkdir_with_parents(user_dir, 0755);
  }
  tb_log("User Data: Loading at %s", user_dir);
  errands_data_migrate_from_46();
  ldata = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)errands_data_free);
  tdata = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)errands_data_free);
  g_autoptr(GDir) dir = g_dir_open(user_dir, 0, NULL);
  if (!dir) return;
  const char *filename;
  while ((filename = g_dir_read_name(dir))) {
    char *is_ics = strstr(filename, ".ics");
    if (is_ics) {
      g_autofree gchar *path = g_build_filename(user_dir, filename, NULL);
      g_autofree gchar *content = tb_read_file_to_string(path);
      if (content) {
        tb_log("User Data: Loading calendar %s", path);
        icalcomponent *calendar = icalparser_parse_string(content);
        if (calendar) {
          // Delete file if calendar deleted and sync is off
          if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b &&
              errands_data_get_bool(calendar, DATA_PROP_DELETED)) {
            tb_log("User Data: Calendar was deleted. Removing %s", path);
            icalcomponent_free(calendar);
            remove(path);
            continue;
          }
          g_hash_table_insert(ldata, strdup(errands_data_get_str(calendar, DATA_PROP_LIST_UID)), calendar);
          // Load tasks
          icalcomponent *c;
          for (c = icalcomponent_get_first_component(calendar, ICAL_VTODO_COMPONENT); c != 0;
               c = icalcomponent_get_next_component(calendar, ICAL_VTODO_COMPONENT))
            g_hash_table_insert(tdata, strdup(errands_data_get_str(c, DATA_PROP_LIST_UID)), c);
        }
      }
    }
  }
  // g_timeout_add_seconds(10, G_SOURCE_FUNC(write_lists), NULL);
}

void errands_data_write_list(ListData *data) {
  // tb_ptr_array_add(&lists_to_write, data);
  g_idle_add_once((GSourceOnceFunc)write_list, data);
}
