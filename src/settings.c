#include "settings.h"
#include "utils.h"

#define JSON_H_IMPLEMENTATION
#include "vendor/json.h"

// Save settings with cooldown period of 1s.
static void errands_settings_save();

// --- GLOBAL SETTINGS VARIABLES --- //

static const char *settings_path;
static time_t last_save_time = 0;
static bool pending_save = false;

static JSON *settings = NULL;

const char *const settings_keys[] = {
    "last_list_uid", "maximized",     "show_completed", "sort_by",       "sync",         "sync_provider",
    "sync_url",      "sync_username", "tags",           "window_height", "window_width",
};

// --- LOADING --- //

void errands_settings_load_default() {
  tb_log("Settings: Create default configuration");
  settings = json_object_new();
  json_object_add(settings, settings_keys[0], json_string_new(""));
  json_object_add(settings, settings_keys[1], json_bool_new(false));
  json_object_add(settings, settings_keys[2], json_bool_new(true));
  json_object_add(settings, settings_keys[3], json_int_new(SORT_TYPE_CREATION_DATE));
  json_object_add(settings, settings_keys[4], json_bool_new(false));
  json_object_add(settings, settings_keys[5], json_string_new("caldav"));
  json_object_add(settings, settings_keys[6], json_string_new(""));
  json_object_add(settings, settings_keys[7], json_string_new(""));
  json_object_add(settings, settings_keys[8], json_string_new(""));
  json_object_add(settings, settings_keys[9], json_int_new(600));
  json_object_add(settings, settings_keys[10], json_int_new(800));
}

void errands_settings_load_user() {
  tb_log("Settings: Load user configuration");
  char *json = read_file_to_string(settings_path);
  if (!json) return;
  JSON *json_parsed = json_parse(json);
  if (!json_parsed) return;
  for (size_t i = 0; i < G_N_ELEMENTS(settings_keys); i++) {
    JSON *node = json_object_get(json_parsed, settings_keys[i]);
    JSON *dup = json_dup(node);
    json_object_add(settings, settings_keys[i], dup);
  }
  json_free(json_parsed);
  free(json);
}

// TODO: Migrate from GSettings to settings.json
void errands_settings_migrate() { tb_log("Settings: Migrate to settings.json"); }

void errands_settings_init() {
  tb_log("Settings: Initialize");
  settings_path = g_build_filename(g_get_user_data_dir(), "errands", "settings.json", NULL);
  errands_settings_load_default();
  if (g_file_test(settings_path, G_FILE_TEST_EXISTS)) errands_settings_load_user();
  else errands_settings_migrate();
  errands_settings_save();
}

// --- GET / SET SETTINGS --- //

ErrandsSetting errands_settings_get(const char *key, ErrandsSettingType type) {
  ErrandsSetting setting = {0};
  JSON *res = json_object_get(settings, key);
  if (type == SETTING_TYPE_INT) setting.i = res->int_val;
  else if (type == SETTING_TYPE_BOOL) setting.b = res->bool_val;
  else if (type == SETTING_TYPE_STRING) setting.s = res->string_val;
  return setting;
}

void errands_settings_set_string(const char *key, const char *value) {
  JSON *res = json_object_get(settings, key);
  json_replace_string(&res->string_val, value);
  errands_settings_save();
}

void errands_settings_set_int(const char *key, int value) {
  JSON *res = json_object_get(settings, key);
  res->int_val = value;
  errands_settings_save();
}

void errands_settings_set_bool(const char *key, bool value) {
  JSON *res = json_object_get(settings, key);
  res->bool_val = value;
  errands_settings_save();
}

// Global tags

GStrv errands_settings_get_tags() {
  JSON *res = json_object_get(settings, "tags");
  return g_strsplit(res->string_val, ",", -1);
}

void errands_settings_set_tags(GStrv tags) {
  g_autofree char *value = g_strjoinv(",", tags);
  JSON *res = json_object_get(settings, "tags");
  json_replace_string(&res->string_val, value);
  errands_settings_save();
}

void errands_settings_add_tag(const char *tag) {
  JSON *res = json_object_get(settings, "tags");
  g_auto(GStrv) tags = g_strsplit(res->string_val, ",", -1);
  if (!g_strv_contains((const char *const *)tags, tag)) {
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    g_strv_builder_addv(builder, (const char **)tags);
    g_strv_builder_add(builder, tag);
    g_auto(GStrv) new_tags = g_strv_builder_end(builder);
    errands_settings_set_tags(new_tags);
  }
}

void errands_settings_remove_tag(const char *tag) {
  JSON *res = json_object_get(settings, "tags");
  g_auto(GStrv) tags = g_strsplit(res->string_val, ",", -1);
  if (g_strv_contains((const char *const *)tags, tag)) {
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    for (int i = 0; tags[i]; i++)
      if (strcmp(tags[i], tag)) g_strv_builder_add(builder, tags[i]);
    g_auto(GStrv) new_tags = g_strv_builder_end(builder);
    errands_settings_set_tags(new_tags);
  }
}

// --- SAVING SETTINGS --- //

static void perform_save() {
  tb_log("Settings: Save");
  g_autoptr(GError) error = NULL;
  char *json = json_print(settings);
  if (!g_file_set_contents(settings_path, json, -1, &error))
    tb_log("Settings: Failed to save settings: %s", error->message);
  last_save_time = time(NULL);
  pending_save = false;
  free(json);
}

void errands_settings_save() {
  double diff = difftime(time(NULL), last_save_time);
  if (pending_save) return;
  if (diff < 1.0f) {
    pending_save = true;
    g_timeout_add_seconds_once(1, (GSourceOnceFunc)perform_save, NULL);
    return;
  } else {
    pending_save = true;
    perform_save();
    return;
  }
}
