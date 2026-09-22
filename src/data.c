#include "data.h"
#include "glib.h"
#include "settings.h"
#include "task-list-item.h"
#include "utils.h"

#include "vendor/json.h"

AUTOPTR_DEFINE(JSON, json_free)

// ---------- GLOBALS ---------- //

GListStore *task_lists_model = NULL;

gchar *user_dir, *calendars_dir, *backups_dir;

// ---------- PRIVATE FUNCTIONS ---------- //

static void create_backup() {
  g_mkdir_with_parents(backups_dir, 0755);
  // Count files in backups_dir
  autofree char *out = NULL;
  int res = cmd_run_stdout(tmp_str_printf("ls %s | wc -l", backups_dir), &out);
  if (res != 0 && !out) return;
  // Remove oldest backup
  if (STR_TO_UL(out) >= 20) {
    g_message("User Data: Removing oldest backup");
    system(tmp_str_printf("rm -f $(find %s/* -type f | sort | head -n 1)", backups_dir));
  }
  // Create backup
  time_t t = TIME_NOW;
  struct tm *tm = localtime(&t);
  char time_str[15];
  strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", tm);
  res = system(tmp_str_printf("cd %s && tar -cJf backups/%s.tar.xz calendars", user_dir, time_str));
  g_message("User Data: %s backup at %s", res == 0 ? "Created" : "Failed to create",
            tmp_str_printf("%s/%s.tar.xz", backups_dir, time_str));
}

static void migrate_from_46() {
  g_autofree gchar *old_data_file = g_build_filename(user_dir, "data.json", NULL);
  if (!g_file_test(old_data_file, G_FILE_TEST_EXISTS)) return;
  g_message("User Data: Migrate from 46.x");
  g_autofree gchar *contents = read_file_to_string(old_data_file);
  g_autoptr(GError) error = NULL;
  g_file_get_contents(old_data_file, &contents, NULL, &error);
  // Read file contents
  if (!contents) {
    g_message("User Data: Failed to read old data file at %s. %s", old_data_file, error->message);
    return;
  }
  // Parse JSON
  autoptr(JSON) root = json_parse(contents);
  if (!root) return;
  // Process lists
  JSON *cal_arr = json_object_get(root, "lists");
  JSON *cal_item = NULL;
  for (cal_item = cal_arr->child; cal_item; cal_item = cal_item->next) {
    JSON *list_uid_item = json_object_get(cal_item, "uid");

    g_autoptr(ErrandsTaskListItem) item = errands_task_list_item_create(
        list_uid_item->string_val, json_object_get(cal_item, "name")->string_val,
        json_object_get(cal_item, "color")->string_val, json_object_get(cal_item, "deleted")->bool_val,
        json_object_get(cal_item, "synced")->bool_val);

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
      if (rrule_item) {
        struct icalrecurrencetype *rrule = icalrecurrencetype_new_from_string(rrule_item->string_val);
        errands_data_set_rrule(ical, rrule);
        icalrecurrencetype_unref(rrule);
      }
      if (start_date_item) errands_data_set_start(ical, icaltime_from_string(start_date_item->string_val));
      if (text_item) errands_data_set_text(ical, text_item->string_val);
      if (uid_item) errands_data_set_uid(ical, uid_item->string_val);
      errands_data_set_attachments(ical, attachments);
      errands_data_set_color(ical, color_item->string_val);
      errands_data_set_deleted(ical, deleted_item->bool_val);
      errands_data_set_notified(ical, notified_item->bool_val);
      icalcomponent_add_component(item->ical, ical);
    }
    errands_task_list_item_save(item);
  }
  remove(old_data_file);
}

static icalproperty *get_x_prop(icalcomponent *ical, const char *xprop, const char *default_val) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_X_PROPERTY);
  while (property) {
    const char *name = icalproperty_get_x_name(property);
    if (name && g_str_equal(name, xprop)) return property;
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
  task_lists_model = g_list_store_new(ERRANDS_TYPE_TASK_LIST_ITEM);
  user_dir = g_build_filename(g_get_user_data_dir(), "errands", NULL);
  calendars_dir = g_build_filename(user_dir, "calendars", NULL);
  backups_dir = g_build_filename(user_dir, "backups", NULL);

  g_mkdir_with_parents(calendars_dir, 0755);
  migrate_from_46();
  create_backup();

  // --- Load lists --- //

  g_autoptr(GDir) dir = g_dir_open(calendars_dir, 0, NULL);
  if (!dir) return;
  TIMER_START;
  const char *filename = NULL;
  int n = 0;
  while ((filename = g_dir_read_name(dir))) {
    if (!g_str_has_suffix(filename, ".ics")) continue;
    g_autofree gchar *path = g_build_filename(calendars_dir, filename, NULL);
    g_autoptr(ErrandsTaskListItem) item = errands_task_list_item_load_from_ics(path);
    if (!item) continue;
    // Delete file if calendar deleted
    if (errands_data_get_deleted(item->ical)) {
      if ((errands_settings_get(SETTING_SYNC).b && errands_data_get_synced(item->ical)) ||
          !errands_settings_get(SETTING_SYNC).b) {
        g_message("User Data: Calendar was deleted. Removing %s", path);
        remove(path);
        continue;
      }
    }
    g_list_store_append(task_lists_model, item);
    errands_task_list_item_remove_deleted_tasks(item);
    g_message("User Data: Loaded calendar %s", path);
    n++;
  }
  g_message("User Data: Loaded %d task-lists in %f sec.", n, TIMER_ELAPSED_MS);
}

void errands_data_cleanup(void) {
  g_message("User Data: Cleanup");
  if (user_dir) g_free(user_dir);
  if (calendars_dir) g_free(calendars_dir);
  if (backups_dir) g_free(backups_dir);
  g_list_store_remove_all(task_lists_model);
  g_object_unref(task_lists_model);
}

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

const char *errands_data_get_color(icalcomponent *ical) {
  if (icalcomponent_isa(ical) == ICAL_VCALENDAR_COMPONENT)
    return get_x_prop_value(ical, "X-APPLE-CALENDAR-COLOR", NULL);
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
const char *errands_data_get_uid(icalcomponent *ical) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_UID_PROPERTY);
  return property ? icalproperty_get_uid(property) : NULL;
}

void errands_data_set_notes(icalcomponent *ical, const char *value) {
  if (!value || STR_EQUAL(value, ""))
    icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_DESCRIPTION_PROPERTY));
  else icalcomponent_set_description(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_color(icalcomponent *ical, const char *value) {
  const char *color = value;
  if (!color || g_str_equal(color, "")) color = generate_hex_as_str();
  g_autofree char *fixed_color = NULL;
  if (!g_str_has_prefix(color, "#")) {
    if (strlen(color) != 7) {
      fixed_color = g_strndup(color, 7);
      color = fixed_color;
    }
  }
  if (icalcomponent_isa(ical) == ICAL_VCALENDAR_COMPONENT) {
    set_x_prop_value(ical, "X-APPLE-CALENDAR-COLOR", color && !g_str_equal(color, "") ? color : NULL);
  } else {
    if (!value || g_str_equal(value, ""))
      icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_COLOR_PROPERTY));
    icalproperty *property = icalcomponent_get_first_property(ical, ICAL_COLOR_PROPERTY);
    if (property) icalproperty_set_color(property, color);
    else icalcomponent_add_property(ical, icalproperty_new_color(color));
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
  if (!value || g_str_equal(value, ""))
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
  if (!value || g_str_equal(value, ""))
    icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_SUMMARY_PROPERTY));
  else icalcomponent_set_summary(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}
void errands_data_set_uid(icalcomponent *ical, const char *value) {
  if (!value || g_str_equal(value, ""))
    icalcomponent_remove_property(ical, icalcomponent_get_first_property(ical, ICAL_UID_PROPERTY));
  else {
    icalproperty *prop = icalcomponent_get_first_property(ical, ICAL_UID_PROPERTY);
    if (prop) return;
    icalcomponent_add_property(ical, icalproperty_new_uid(value));
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());
}

// --- RRULE --- //

struct icalrecurrencetype *errands_data_get_rrule(icalcomponent *ical) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_RRULE_PROPERTY);
  return property ? icalproperty_get_rrule(property) : NULL;
}

bool errands_data_set_rrule(icalcomponent *ical, struct icalrecurrencetype *value) {
  struct icalrecurrencetype *old_rrule = errands_data_get_rrule(ical);
  if (icalrecurrencetype_compare(value, old_rrule)) return false;
  bool is_no_recurrence = value && value->freq == ICAL_NO_RECURRENCE;
  icalproperty *prop = icalcomponent_get_first_property(ical, ICAL_RRULE_PROPERTY);
  if (is_no_recurrence) {
    if (prop) icalcomponent_remove_property(ical, prop);
  } else {
    if (prop) icalproperty_set_rrule(prop, value);
    else icalcomponent_add_property(ical, icalproperty_new_rrule(value));
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, icaltime_get_date_time_now());

  return true;
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
