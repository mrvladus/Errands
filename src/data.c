#include "data.h"
#include "settings.h"
#include "state.h"
#include "task-list-item.h"
#include "utils.h"

#include <json-glib/json-glib.h>

// ---------- GLOBALS ---------- //

GListStore *task_lists_model = NULL;

gchar *user_dir, *calendars_dir;

// ---------- PRIVATE FUNCTIONS ---------- //

static void create_backup() {
  g_autofree gchar *backups_dir = g_build_filename(user_dir, "backups", NULL);
  g_mkdir_with_parents(backups_dir, 0755);
  // Count files in backups_dir
  char *out = NULL;
  int res = cmd_run_stdout(tmp_str_printf("ls %s | wc -l", backups_dir), &out);
  if (res != 0 && !out) return;
  // Remove oldest backup
  if (strtoul(out, NULL, 10) >= 20) {
    g_message("User Data: Removing oldest backup");
    system(tmp_str_printf("rm -f $(find %s/* -type f | sort | head -n 1)", backups_dir));
  }
  free(out);
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
  g_autofree gchar *filename = g_build_filename(user_dir, "data.json", NULL);
  if (!g_file_test(filename, G_FILE_TEST_EXISTS)) return;
  g_message("User Data: Migrate from 46.x");
  g_autoptr(GError) error = NULL;
  g_autoptr(JsonParser) parser = json_parser_new();
  json_parser_load_from_file(parser, filename, &error);
  if (error != NULL) {
    g_critical("Unable to parse '%s': %s", filename, error->message);
    return;
  }

  JsonObject *root = json_node_get_object(json_parser_get_root(parser));

  // Process tags
  JsonArray *tags_arr = json_object_get_array_member(root, "tags");
  g_autoptr(GStrvBuilder) tags_builder = g_strv_builder_new();
  for_range(i, 0, json_array_get_length(tags_arr)) g_strv_builder_add(tags_builder, json_array_get_string_element(tags_arr, i));
  GStrv tags = g_strv_builder_end(tags_builder);
  errands_settings_set_tags(state.settings, tags);

  // Process lists
  JsonArray *lists = json_object_get_array_member(root, "lists");
  for_range(i, 0, json_array_get_length(lists)) {
    JsonObject *list = json_node_get_object(json_array_get_element(lists, i));
    const gchar *list_uid = json_object_get_string_member(list, "uid");
    const gchar *list_name = json_object_get_string_member(list, "name");
    const gchar *list_color = json_object_get_string_member(list, "color");
    gboolean list_deleted = json_object_get_boolean_member(list, "deleted");
    gboolean list_synced = json_object_get_boolean_member(list, "synced");

    g_autoptr(ErrandsTaskListItem) list_item =
        errands_task_list_item_create(list_uid, list_name, list_color, list_deleted, list_synced);

    // Process tasks
    JsonArray *tasks = json_object_get_array_member(root, "tasks");
    for_range(j, 0, json_array_get_length(tasks)) {
      JsonObject *task = json_node_get_object(json_array_get_element(tasks, j));
      const gchar *task_list_uid = json_object_get_string_member(task, "list_uid");
      if (!g_str_equal(task_list_uid, list_uid)) continue;

      // Process attachments
      JsonArray *task_attachments_arr = json_object_get_array_member(task, "attachments");
      g_autoptr(GStrvBuilder) attachments_builder = g_strv_builder_new();
      for_range(k, 0, json_array_get_length(task_attachments_arr))
          g_strv_builder_add(attachments_builder, json_array_get_string_element(task_attachments_arr, k));
      g_auto(GStrv) attachments = g_strv_builder_end(attachments_builder);

      // Process tags
      JsonArray *task_tags_arr = json_object_get_array_member(task, "tags");
      g_autoptr(GStrvBuilder) tags_builder = g_strv_builder_new();
      for_range(k, 0, json_array_get_length(task_tags_arr))
          g_strv_builder_add(tags_builder, json_array_get_string_element(task_tags_arr, k));
      g_auto(GStrv) tags = g_strv_builder_end(tags_builder);

      // Process properties
      const gchar *uid = json_object_get_string_member(task, "uid");
      const gchar *text = json_object_get_string_member(task, "text");
      const gchar *color = json_object_get_string_member(task, "color");
      const gchar *parent = json_object_get_string_member(task, "parent");
      const gchar *changed = json_object_get_string_member(task, "changed_at");
      const gchar *created = json_object_get_string_member(task, "created_at");
      const gchar *due = json_object_get_string_member(task, "due_date");
      const gchar *notes = json_object_get_string_member(task, "notes");
      const gchar *rrule = json_object_get_string_member(task, "rrule");
      const gchar *start = json_object_get_string_member(task, "start_date");
      gboolean completed = json_object_get_boolean_member(task, "completed");
      gboolean deleted = json_object_get_boolean_member(task, "deleted");
      gboolean notified = json_object_get_boolean_member(task, "notified");
      gboolean synced = json_object_get_boolean_member(task, "synced");
      gint64 priority = json_object_get_int_member(task, "priority");
      gint64 percent_complete = json_object_get_int_member(task, "percent_complete");

      // Create iCalendar event
      g_autoptr(ICalComponent) ical = i_cal_component_new_vtodo();

      if (completed) errands_data_set_completed(ical, i_cal_time_get_date_time_now());
      if (tags) errands_data_set_tags(ical, tags);
      if (changed) errands_data_set_changed(ical, i_cal_time_new_from_string(changed));
      if (created) errands_data_set_created(ical, i_cal_time_new_from_string(created));
      if (due) errands_data_set_due(ical, i_cal_time_new_from_string(due));
      if (notes) errands_data_set_notes(ical, notes);
      if (parent) errands_data_set_parent(ical, parent);
      if (percent_complete) errands_data_set_percent(ical, percent_complete);
      if (priority) errands_data_set_priority(ical, priority);
      if (rrule) {
        ICalRecurrence *rrule_ = i_cal_recurrence_new_from_string(rrule);
        errands_data_set_rrule(ical, rrule_);
        i_cal_recurrence_unref(rrule_);
      }
      if (start) errands_data_set_start(ical, i_cal_time_new_from_string(start));
      if (text) errands_data_set_text(ical, text);
      if (uid) errands_data_set_uid(ical, uid);
      errands_data_set_attachments(ical, attachments);
      errands_data_set_color(ical, color);
      errands_data_set_deleted(ical, deleted);
      errands_data_set_notified(ical, notified);
      errands_data_set_synced(ical, synced);
      i_cal_component_add_component(list_item->ical, ical);
    }
    errands_task_list_item_save(list_item);
  }
  remove(filename);
  g_message("User Data: Migrated successfully");
}

static ICalProperty *get_x_prop(ICalComponent *ical, const char *xprop, const char *default_val) {
  ICalProperty *property = i_cal_component_get_first_property(ical, I_CAL_X_PROPERTY);
  while (property) {
    const char *name = i_cal_property_get_x_name(property);
    if (name && g_str_equal(name, xprop)) return property;
    g_object_unref(property);
    property = i_cal_component_get_next_property(ical, I_CAL_X_PROPERTY);
  }
  if (!default_val) return NULL;
  property = i_cal_property_new_x(default_val);
  i_cal_property_set_x_name(property, xprop);
  i_cal_component_add_property(ical, property);
  return property;
}

static gchar *get_x_prop_value(ICalComponent *ical, const char *xprop, const char *default_val) {
  ICalProperty *property = get_x_prop(ical, xprop, default_val);
  if (!property) return NULL;
  return i_cal_property_get_value_as_string(property);
}

static void set_x_prop_value(ICalComponent *ical, const char *xprop, const char *val) {
  ICalProperty *property = get_x_prop(ical, xprop, val);
  if (!val && property) i_cal_component_remove_property(ical, property);
  else i_cal_property_set_x(property, val);
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_data_init() {
  task_lists_model = g_list_store_new(ERRANDS_TYPE_TASK_LIST_ITEM);
  user_dir = g_build_filename(g_get_user_data_dir(), "errands", NULL);
  calendars_dir = g_build_filename(user_dir, "calendars", NULL);

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
      if ((errands_settings_get_sync_enabled(state.settings) && errands_data_get_synced(item->ical)) ||
          !errands_settings_get_sync_enabled(state.settings)) {
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
  g_list_store_remove_all(task_lists_model);
  g_object_unref(task_lists_model);
}

// ---------- PROPERTIES ---------- //

// --- BOOL --- //

bool errands_data_get_cancelled(ICalComponent *ical) {
  ICalPropertyStatus status = i_cal_component_get_status(ical);
  return status == I_CAL_STATUS_CANCELLED;
}

bool errands_data_get_deleted(ICalComponent *ical) {
  g_autofree gchar *val = get_x_prop_value(ical, "X-ERRANDS-DELETED", "0");
  return STR_TO_BOOL(val);
}

bool errands_data_get_notified(ICalComponent *ical) {
  g_autofree gchar *val = get_x_prop_value(ical, "X-ERRANDS-NOTIFIED", "0");
  return STR_TO_BOOL(val);
}

bool errands_data_get_synced(ICalComponent *ical) {
  g_autofree gchar *val = get_x_prop_value(ical, "X-ERRANDS-SYNCED", "0");
  return STR_TO_BOOL(val);
}

bool errands_data_is_completed(ICalComponent *ical) {
  g_autoptr(ICalTime) time = errands_data_get_completed(ical);
  return !i_cal_time_is_null_time(time);
}

bool errands_data_is_due(ICalComponent *ical) {
  g_autoptr(ICalTime) due_time = errands_data_get_due(ical);
  if (i_cal_time_is_null_time(due_time)) return false;
  bool is_due = false;
  if (i_cal_time_is_date(due_time)) {
    g_autoptr(ICalTime) today = i_cal_time_new_today();
    is_due = i_cal_time_compare_date_only(due_time, today) < 1;
  } else {
    g_autoptr(ICalTime) now = i_cal_time_get_date_time_now();
    is_due = i_cal_time_compare(due_time, now) < 1;
  }
  return is_due;
}

void errands_data_set_cancelled(ICalComponent *ical, bool value) {
  i_cal_component_set_status(ical, value ? I_CAL_STATUS_CANCELLED : I_CAL_STATUS_NEEDSACTION);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}

void errands_data_set_deleted(ICalComponent *ical, bool value) {
  set_x_prop_value(ical, "X-ERRANDS-DELETED", BOOL_TO_STR_NUM(value));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}

void errands_data_set_notified(ICalComponent *ical, bool value) {
  set_x_prop_value(ical, "X-ERRANDS-NOTIFIED", BOOL_TO_STR_NUM(value));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}

void errands_data_set_synced(ICalComponent *ical, bool value) {
  set_x_prop_value(ical, "X-ERRANDS-SYNCED", BOOL_TO_STR_NUM(value));
}

// --- INT --- //

int errands_data_get_percent(ICalComponent *ical) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_PERCENTCOMPLETE_PROPERTY);
  return property ? i_cal_property_get_percentcomplete(property) : 0;
}

int errands_data_get_priority(ICalComponent *ical) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_PRIORITY_PROPERTY);
  return property ? i_cal_property_get_priority(property) : 0;
}

void errands_data_set_percent(ICalComponent *ical, int value) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_PERCENTCOMPLETE_PROPERTY);
  if (property) i_cal_property_set_percentcomplete(property, value);
  else i_cal_component_add_property(ical, i_cal_property_new_percentcomplete(value));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}

void errands_data_set_priority(ICalComponent *ical, int value) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_PRIORITY_PROPERTY);
  if (property) i_cal_property_set_percentcomplete(property, value);
  else i_cal_component_add_property(ical, i_cal_property_new_priority(value));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}

// --- STRING --- //

const char *errands_data_get_color(ICalComponent *ical) {
  if (i_cal_component_isa(ical) == I_CAL_VCALENDAR_COMPONENT) return get_x_prop_value(ical, "X-APPLE-CALENDAR-COLOR", NULL);
  else {
    g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_COLOR_PROPERTY);
    return property ? i_cal_property_get_color(property) : NULL;
  }
}

const char *errands_data_get_list_name(ICalComponent *ical) { return get_x_prop_value(ical, "X-WR-CALNAME", "Untitled"); }

const char *errands_data_get_list_description(ICalComponent *ical) { return get_x_prop_value(ical, "X-WR-CALDESC", NULL); }

const char *errands_data_get_notes(ICalComponent *ical) { return i_cal_component_get_description(ical); }

const char *errands_data_get_parent(ICalComponent *ical) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_RELATEDTO_PROPERTY);
  return property ? i_cal_property_get_relatedto(property) : NULL;
}

const char *errands_data_get_text(ICalComponent *ical) { return i_cal_component_get_summary(ical); }

const char *errands_data_get_uid(ICalComponent *ical) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_UID_PROPERTY);
  return property ? i_cal_property_get_uid(property) : NULL;
}

void errands_data_set_notes(ICalComponent *ical, const char *value) {
  if (!value || g_str_equal(value, "")) {
    g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_DESCRIPTION_PROPERTY);
    i_cal_component_remove_property(ical, property);
  } else i_cal_component_set_description(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}

void errands_data_set_color(ICalComponent *ical, const char *value) {
  const char *color = value;
  g_autofree char *fixed_color = NULL;
  if (color && !g_str_has_prefix(color, "#")) {
    if (strlen(color) != 7) {
      fixed_color = g_strndup(color, 7);
      color = fixed_color;
    }
  }
  if (i_cal_component_isa(ical) == I_CAL_VCALENDAR_COMPONENT) {
    if (!color || g_str_equal(color, "")) color = generate_hex_as_str();
    set_x_prop_value(ical, "X-APPLE-CALENDAR-COLOR", color);
  } else {
    if (!value || g_str_equal(value, "")) {
      g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_COLOR_PROPERTY);
      if (property) i_cal_component_remove_property(ical, property);
    }
    g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_COLOR_PROPERTY);
    if (property) i_cal_property_set_color(property, color);
    else i_cal_component_add_property(ical, i_cal_property_new_color(color));
    errands_data_set_changed(ical, i_cal_time_get_date_time_now());
  }
  errands_data_set_synced(ical, false);
}
void errands_data_set_list_name(ICalComponent *ical, const char *value) {
  set_x_prop_value(ical, "X-WR-CALNAME", value);
  errands_data_set_synced(ical, false);
}
void errands_data_set_list_description(ICalComponent *ical, const char *value) {
  set_x_prop_value(ical, "X-WR-CALDESC", value);
  errands_data_set_synced(ical, false);
}
void errands_data_set_parent(ICalComponent *ical, const char *value) {
  if (!value || g_str_equal(value, ""))
    i_cal_component_remove_property(ical, i_cal_component_get_first_property(ical, I_CAL_RELATEDTO_PROPERTY));
  else {
    g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_RELATEDTO_PROPERTY);
    if (property) i_cal_property_set_relatedto(property, value);
    else i_cal_component_add_property(ical, i_cal_property_new_relatedto(value));
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}
void errands_data_set_text(ICalComponent *ical, const char *value) {
  if (!value || g_str_equal(value, ""))
    i_cal_component_remove_property(ical, i_cal_component_get_first_property(ical, I_CAL_SUMMARY_PROPERTY));
  else i_cal_component_set_summary(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}
void errands_data_set_uid(ICalComponent *ical, const char *value) {
  if (!value || g_str_equal(value, ""))
    i_cal_component_remove_property(ical, i_cal_component_get_first_property(ical, I_CAL_UID_PROPERTY));
  else {
    g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_UID_PROPERTY);
    if (property) return;
    i_cal_component_add_property(ical, i_cal_property_new_uid(value));
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}

// --- RRULE --- //

ICalRecurrence *errands_data_get_rrule(ICalComponent *ical) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_RRULE_PROPERTY);
  return property ? i_cal_property_get_rrule(property) : NULL;
}

bool errands_data_set_rrule(ICalComponent *ical, ICalRecurrence *value) {
  bool is_no_recurrence = !value || (value && i_cal_recurrence_get_freq(value) == I_CAL_NO_RECURRENCE);
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_RRULE_PROPERTY);
  if (is_no_recurrence) {
    if (property) i_cal_component_remove_property(ical, property);
  } else {
    if (property) i_cal_property_set_rrule(property, value);
    else i_cal_component_add_property(ical, i_cal_property_new_rrule(value));
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());

  return true;
}

// --- STRV --- //

GStrv errands_data_get_attachments(ICalComponent *ical) {
  g_autofree gchar *val = get_x_prop_value(ical, "X-ERRANDS-ATTACHMENTS", NULL);
  return val ? g_strsplit(val, ",", -1) : NULL;
}

GStrv errands_data_get_tags(ICalComponent *ical) {
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for (ICalProperty *p = i_cal_component_get_first_property(ical, I_CAL_CATEGORIES_PROPERTY); p != 0;
       p = i_cal_component_get_next_property(ical, I_CAL_CATEGORIES_PROPERTY)) {
    g_strv_builder_add(builder, i_cal_property_get_value_as_string(p));
    g_object_unref(p);
  }
  return g_strv_builder_end(builder);
}

void errands_data_set_attachments(ICalComponent *ical, GStrv value) {
  g_autofree gchar *str = g_strjoinv(",", value);
  set_x_prop_value(ical, "X-ERRANDS-ATTACHMENTS", str);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}
void errands_data_set_tags(ICalComponent *ical, GStrv value) {
  for (ICalProperty *p = i_cal_component_get_first_property(ical, I_CAL_CATEGORIES_PROPERTY); p != 0;
       p = i_cal_component_get_next_property(ical, I_CAL_CATEGORIES_PROPERTY)) {
    i_cal_component_remove_property(ical, p);
    g_object_unref(p);
  }
  for (size_t i = 0; i < g_strv_length(value); i++) i_cal_component_add_property(ical, i_cal_property_new_categories(value[i]));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}

void errands_data_add_tag(ICalComponent *ical, const char *tag) {
  i_cal_component_add_property(ical, i_cal_property_new_categories(tag));
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
}

bool errands_data_remove_tag(ICalComponent *ical, const char *tag) {
  for (ICalProperty *p = i_cal_component_get_first_property(ical, I_CAL_CATEGORIES_PROPERTY); p;
       p = i_cal_component_get_next_property(ical, I_CAL_CATEGORIES_PROPERTY)) {
    g_autofree gchar *val = i_cal_property_get_value_as_string(p);
    if (g_str_equal(tag, val)) {
      i_cal_component_remove_property(ical, p);
      errands_data_set_synced(ical, false);
      errands_data_set_changed(ical, i_cal_time_get_date_time_now());
      return true;
    }
    g_object_unref(p);
  }
  return false;
}

// --- TIME --- //

ICalTime *errands_data_get_changed(ICalComponent *ical) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_LASTMODIFIED_PROPERTY);
  if (property) return i_cal_property_get_lastmodified(property);
  else {
    property = i_cal_component_get_first_property(ical, I_CAL_DTSTAMP_PROPERTY);
    if (property) return i_cal_property_get_dtstamp(property);
  }
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
  g_autoptr(ICalProperty) last_modified = i_cal_component_get_first_property(ical, I_CAL_LASTMODIFIED_PROPERTY);
  return i_cal_property_get_lastmodified(last_modified);
}

ICalTime *errands_data_get_completed(ICalComponent *ical) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_COMPLETED_PROPERTY);
  if (property) return i_cal_property_get_completed(property);
  return i_cal_time_new_null_time();
}

ICalTime *errands_data_get_created(ICalComponent *ical) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_CREATED_PROPERTY);
  if (property) return i_cal_property_get_created(property);
  else {
    property = i_cal_component_get_first_property(ical, I_CAL_DTSTAMP_PROPERTY);
    if (property) return i_cal_property_get_dtstamp(property);
  }
  errands_data_set_created(ical, i_cal_time_get_date_time_now());
  g_autoptr(ICalProperty) created = i_cal_component_get_first_property(ical, I_CAL_CREATED_PROPERTY);
  return i_cal_property_get_created(created);
}

ICalTime *errands_data_get_due(ICalComponent *ical) { return i_cal_component_get_due(ical); }

ICalTime *errands_data_get_end(ICalComponent *ical) { return i_cal_component_get_dtend(ical); }

ICalTime *errands_data_get_start(ICalComponent *ical) { return i_cal_component_get_dtstart(ical); }

void errands_data_set_changed(ICalComponent *ical, ICalTime *value) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_LASTMODIFIED_PROPERTY);
  if (i_cal_time_is_null_time(value) && property) i_cal_component_remove_property(ical, property);
  else {
    if (!property) i_cal_component_add_property(ical, i_cal_property_new_lastmodified(value));
    else i_cal_property_set_lastmodified(property, value);
  }
  g_object_unref(value);
}

void errands_data_set_completed(ICalComponent *ical, ICalTime *value) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_COMPLETED_PROPERTY);
  if (i_cal_time_is_null_time(value) && property) {
    i_cal_component_remove_property(ical, property);
    errands_data_set_percent(ical, 0);
    i_cal_component_set_status(ical, I_CAL_STATUS_NEEDSACTION);
  } else {
    if (!property) i_cal_component_add_property(ical, i_cal_property_new_completed(value));
    else i_cal_property_set_completed(property, value);
    errands_data_set_percent(ical, 100);
    i_cal_component_set_status(ical, I_CAL_STATUS_COMPLETED);
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
  g_object_unref(value);
}

void errands_data_set_created(ICalComponent *ical, ICalTime *value) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_CREATED_PROPERTY);
  if (i_cal_time_is_null_time(value) && property) i_cal_component_remove_property(ical, property);
  else {
    if (!property) i_cal_component_add_property(ical, i_cal_property_new_created(value));
    else i_cal_property_set_created(property, value);
  }
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
  g_object_unref(value);
}

void errands_data_set_due(ICalComponent *ical, ICalTime *value) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_DUE_PROPERTY);
  if (i_cal_time_is_null_time(value) && property) i_cal_component_remove_property(ical, property);
  else i_cal_component_set_due(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
  g_object_unref(value);
}

void errands_data_set_end(ICalComponent *ical, ICalTime *value) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_DTEND_PROPERTY);
  if (i_cal_time_is_null_time(value) && property) i_cal_component_remove_property(ical, property);
  else i_cal_component_set_dtend(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
  g_object_unref(value);
}

void errands_data_set_start(ICalComponent *ical, ICalTime *value) {
  g_autoptr(ICalProperty) property = i_cal_component_get_first_property(ical, I_CAL_DTSTART_PROPERTY);
  if (i_cal_time_is_null_time(value) && property) i_cal_component_remove_property(ical, property);
  else i_cal_component_set_dtstart(ical, value);
  errands_data_set_synced(ical, false);
  errands_data_set_changed(ical, i_cal_time_get_date_time_now());
  g_object_unref(value);
}
