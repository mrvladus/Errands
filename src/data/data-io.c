#include "../settings.h"
#include "../utils.h"
#include "../vendor/cJSON.h"
#include "data.h"
#include <libical/ical.h>

const char *user_dir;

static void errands_data_migrate_from_46() {
  g_autofree gchar *old_data_file = g_build_filename(user_dir, "data.json", NULL);
  if (!g_file_test(old_data_file, G_FILE_TEST_EXISTS)) return;
  LOG("User Data: Migrate");
  g_autoptr(GError) error = NULL;
  gsize length;
  g_autofree gchar *contents = NULL;
  // Read file contents
  if (!g_file_get_contents(old_data_file, &contents, &length, &error)) {
    LOG("User Data: Failed to read old data file: %s", error->message);
    return;
  }
  // Parse JSON with cJSON
  cJSON *root = cJSON_Parse(contents);
  if (!root) {
    LOG("User Data: Failed to parse JSON: %s", cJSON_GetErrorPtr());
    return;
  }
  // Process lists
  cJSON *cal_arr = cJSON_GetObjectItem(root, "lists");
  cJSON *cal_item = NULL;
  cJSON_ArrayForEach(cal_item, cal_arr) {
    cJSON *color_item = cJSON_GetObjectItem(cal_item, "color");
    cJSON *deleted_item = cJSON_GetObjectItem(cal_item, "deleted");
    cJSON *synced_item = cJSON_GetObjectItem(cal_item, "synced");
    cJSON *name_item = cJSON_GetObjectItem(cal_item, "name");
    cJSON *list_uid_item = cJSON_GetObjectItem(cal_item, "uid");

    icalcomponent *calendar = list_data_new(list_uid_item->valuestring, name_item->valuestring, color_item->valuestring,
                                            cJSON_IsTrue(deleted_item), cJSON_IsTrue(synced_item), 0);

    // Process tasks
    cJSON *tasks_arr = cJSON_GetObjectItem(root, "tasks");
    if (!cJSON_IsArray(tasks_arr)) continue;
    cJSON *task_item = NULL;
    cJSON_ArrayForEach(task_item, tasks_arr) {
      cJSON *task_list_uid_item = cJSON_GetObjectItem(task_item, "list_uid");
      const gchar *task_list_uid = task_list_uid_item ? task_list_uid_item->valuestring : NULL;
      if (!task_list_uid || !g_str_equal(task_list_uid, list_uid_item->valuestring)) continue;
      // Process attachments
      cJSON *task_attachments_arr = cJSON_GetObjectItem(task_item, "attachments");
      g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
      cJSON *attachment_item = NULL;
      cJSON_ArrayForEach(attachment_item, task_attachments_arr)
          g_strv_builder_add(builder, attachment_item->valuestring);
      g_auto(GStrv) attachments = g_strv_builder_end(builder);
      // Process tags
      cJSON *task_tags_arr = cJSON_GetObjectItem(task_item, "tags");
      g_autoptr(GStrvBuilder) tags_builder = g_strv_builder_new();
      cJSON *tags_tem = NULL;
      cJSON_ArrayForEach(tags_tem, task_tags_arr) g_strv_builder_add(builder, tags_tem->valuestring);
      g_auto(GStrv) tags = g_strv_builder_end(builder);
      // Extract task properties
      cJSON *color_item = cJSON_GetObjectItem(task_item, "color");
      cJSON *completed_item = cJSON_GetObjectItem(task_item, "completed");
      cJSON *changed_at_item = cJSON_GetObjectItem(task_item, "changed_at");
      cJSON *created_at_item = cJSON_GetObjectItem(task_item, "created_at");
      cJSON *deleted_item = cJSON_GetObjectItem(task_item, "deleted");
      cJSON *due_date_item = cJSON_GetObjectItem(task_item, "due_date");
      cJSON *expanded_item = cJSON_GetObjectItem(task_item, "expanded");
      cJSON *notes_item = cJSON_GetObjectItem(task_item, "notes");
      cJSON *notified_item = cJSON_GetObjectItem(task_item, "notified");
      cJSON *parent_item = cJSON_GetObjectItem(task_item, "parent");
      cJSON *percent_complete_item = cJSON_GetObjectItem(task_item, "percent_complete");
      cJSON *priority_item = cJSON_GetObjectItem(task_item, "priority");
      cJSON *rrule_item = cJSON_GetObjectItem(task_item, "rrule");
      cJSON *start_date_item = cJSON_GetObjectItem(task_item, "start_date");
      cJSON *synced_item = cJSON_GetObjectItem(task_item, "synced");
      cJSON *text_item = cJSON_GetObjectItem(task_item, "text");
      cJSON *toolbar_shown_item = cJSON_GetObjectItem(task_item, "toolbar_shown");
      cJSON *trash_item = cJSON_GetObjectItem(task_item, "trash");
      cJSON *uid_item = cJSON_GetObjectItem(task_item, "uid");
      // Create iCalendar event
      icalcomponent *event = icalcomponent_new(ICAL_VTODO_COMPONENT);
      if (completed_item) errands_data_set_str(event, DATA_PROP_COMPLETED, get_date_time());
      if (tags) errands_data_set_strv(event, DATA_PROP_TAGS, tags);
      if (changed_at_item) errands_data_set_str(event, DATA_PROP_CHANGED, changed_at_item->valuestring);
      if (created_at_item) errands_data_set_str(event, DATA_PROP_CREATED, created_at_item->valuestring);
      if (due_date_item) errands_data_set_str(event, DATA_PROP_DUE, due_date_item->valuestring);
      if (notes_item) errands_data_set_str(event, DATA_PROP_NOTES, notes_item->valuestring);
      if (parent_item) errands_data_set_str(event, DATA_PROP_PARENT, parent_item->valuestring);
      if (percent_complete_item) errands_data_set_int(event, DATA_PROP_PERCENT, percent_complete_item->valueint);
      if (priority_item) errands_data_set_int(event, DATA_PROP_PRIORITY, priority_item->valueint);
      if (rrule_item) errands_data_set_str(event, DATA_PROP_RRULE, rrule_item->valuestring);
      if (start_date_item) errands_data_set_str(event, DATA_PROP_START, start_date_item->valuestring);
      if (text_item) errands_data_set_str(event, DATA_PROP_TEXT, text_item->valuestring);
      if (uid_item) errands_data_set_str(event, DATA_PROP_UID, uid_item->valuestring);
      errands_data_set_strv(event, DATA_PROP_ATTACHMENTS, attachments);
      errands_data_set_str(event, DATA_PROP_COLOR, color_item->valuestring);
      errands_data_set_bool(event, DATA_PROP_DELETED, cJSON_IsTrue(deleted_item));
      errands_data_set_bool(event, DATA_PROP_EXPANDED, cJSON_IsTrue(expanded_item));
      errands_data_set_bool(event, DATA_PROP_NOTIFIED, cJSON_IsTrue(notified_item));
      errands_data_set_bool(event, DATA_PROP_SYNCED, cJSON_IsTrue(synced_item));
      errands_data_set_bool(event, DATA_PROP_TOOLBAR_SHOWN, cJSON_IsTrue(toolbar_shown_item));
      errands_data_set_bool(event, DATA_PROP_TRASH, cJSON_IsTrue(trash_item));
      errands_data_set_str(event, DATA_PROP_LIST_UID, task_list_uid_item->valuestring);
      icalcomponent_add_component(calendar, event);
    }
    // Save calendar to file
    g_autofree gchar *calendar_filename = g_strdup_printf("%s.ics", list_uid_item->valuestring);
    g_autofree gchar *calendar_file_path = g_build_filename(user_dir, calendar_filename, NULL);
    if (!g_file_set_contents(calendar_file_path, icalcomponent_as_ical_string(calendar), -1, &error))
      LOG("User Data: Failed to save calendar to %s: %s", calendar_file_path, error->message);
  }
  // Clean up
  cJSON_Delete(root);
  remove(old_data_file);
}

void errands_data_load_lists() {
  user_dir = g_build_filename(g_get_user_data_dir(), "errands", NULL);
  if (!directory_exists(user_dir)) {
    LOG("User Data: Creating user data directory at %s", user_dir);
    g_mkdir_with_parents(user_dir, 0755);
  }
  LOG("User Data: Loading at %s", user_dir);
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
      g_autofree gchar *content = read_file_to_string(path);
      if (content) {
        LOG("User Data: Loading calendar %s", path);
        icalcomponent *calendar = icalparser_parse_string(content);
        if (calendar) {
          // Delete file if calendar deleted and sync is off
          if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b &&
              errands_data_get_bool(calendar, DATA_PROP_DELETED)) {
            LOG("User Data: Calendar was deleted. Removing %s", path);
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
}

void errands_data_write_list(ListData *data) {
  g_autofree gchar *filename = g_strdup_printf("%s.ics", errands_data_get_str(data, DATA_PROP_LIST_UID));
  g_autofree gchar *path = g_build_filename(user_dir, filename, NULL);
  if (!g_file_set_contents(path, icalcomponent_as_ical_string(data), -1, NULL))
    LOG("User Data: Failed to save list %s", path);
  LOG("User Data: Save list %s", path);
}
