#include "settings.h"

#define JSON_H_IMPLEMENTATION
#include "vendor/json.h"
#include "vendor/toolbox.h"

AUTOPTR_DEFINE(JSON, json_free)

// Save settings with cooldown period of 1s.
static void errands_settings_save();

// --- GLOBAL SETTINGS VARIABLES --- //

static const char *settings_path;
static time_t last_save_time = 0;
static bool pending_save = false;
static JSON *settings = NULL;

static const char *const settings_keys_strs[SETTINGS_COUNT] = {
    [SETTING_LAST_LIST_UID] = "last_list_uid",
    [SETTING_MAXIMIZED] = "maximized",
    [SETTING_SHOW_COMPLETED] = "show_completed",
    [SETTING_SORT_BY] = "sort_by",
    [SETTING_SYNC] = "sync",
    [SETTING_SYNC_PROVIDER] = "sync_provider",
    [SETTING_SYNC_URL] = "sync_url",
    [SETTING_SYNC_USERNAME] = "sync_username",
    [SETTING_TAGS] = "tags",
    [SETTING_WINDOW_HEIGHT] = "window_height",
    [SETTING_WINDOW_WIDTH] = "window_width",
};

#define SETTING(key) settings_keys_strs[key]

// --- LOADING --- //

void errands_settings_load_default() {
  LOG("Settings: Create default configuration");
  settings = json_object_new();
  json_object_add(settings, SETTING(SETTING_LAST_LIST_UID), json_string_new(""));
  json_object_add(settings, SETTING(SETTING_MAXIMIZED), json_bool_new(false));
  json_object_add(settings, SETTING(SETTING_SHOW_COMPLETED), json_bool_new(true));
  json_object_add(settings, SETTING(SETTING_SORT_BY), json_int_new(SORT_TYPE_CREATION_DATE));
  json_object_add(settings, SETTING(SETTING_SYNC), json_bool_new(false));
  json_object_add(settings, SETTING(SETTING_SYNC_PROVIDER), json_string_new("caldav"));
  json_object_add(settings, SETTING(SETTING_SYNC_URL), json_string_new(""));
  json_object_add(settings, SETTING(SETTING_SYNC_USERNAME), json_string_new(""));
  json_object_add(settings, SETTING(SETTING_TAGS), json_string_new(""));
  json_object_add(settings, SETTING(SETTING_WINDOW_HEIGHT), json_int_new(600));
  json_object_add(settings, SETTING(SETTING_WINDOW_WIDTH), json_int_new(800));
}

void errands_settings_load_user() {
  LOG("Settings: Load user configuration");
  autofree char *json = read_file_to_string(settings_path);
  if (!json) return;
  autoptr(JSON) json_parsed = json_parse(json);
  if (!json_parsed) return;
  for (size_t i = 0; i < SETTINGS_COUNT; i++) {
    JSON *node = json_object_get(json_parsed, settings_keys_strs[i]);
    JSON *dup = json_dup(node);
    json_object_add(settings, settings_keys_strs[i], dup);
  }
}

// TODO: Migrate from GSettings to settings.json
void errands_settings_migrate() { LOG("Settings: Migrate to settings.json"); }

void errands_settings_init() {
  LOG("Settings: Initialize");
  settings_path = g_build_filename(g_get_user_data_dir(), "errands", "settings.json", NULL);
  errands_settings_load_default();
  if (file_exists(settings_path)) errands_settings_load_user();
  else errands_settings_migrate();
  errands_settings_save();
}

// --- GET / SET SETTINGS --- //

ErrandsSetting errands_settings_get(ErrandsSettingsKey key) {
  JSON *res = json_object_get(settings, SETTING(key));
  ErrandsSetting out = {0};
  switch (key) {
  case SETTING_LAST_LIST_UID: out.s = res->string_val; break;
  case SETTING_MAXIMIZED: out.b = res->bool_val; break;
  case SETTING_SHOW_COMPLETED: out.b = res->bool_val; break;
  case SETTING_SORT_BY: out.i = res->int_val; break;
  case SETTING_SYNC: out.b = res->bool_val; break;
  case SETTING_SYNC_PROVIDER: out.s = res->string_val; break;
  case SETTING_SYNC_URL: out.s = res->string_val; break;
  case SETTING_SYNC_USERNAME: out.s = res->string_val; break;
  case SETTING_TAGS: out.s = res->string_val; break;
  case SETTING_WINDOW_HEIGHT: out.i = res->int_val; break;
  case SETTING_WINDOW_WIDTH: out.i = res->int_val; break;
  case SETTINGS_COUNT: break;
  }
  return out;
}

void errands_settings_set(const char *key, ErrandsSettingType type, void *value) {
  JSON *res = json_object_get(settings, key);
  switch (type) {
  case SETTING_TYPE_BOOL: res->bool_val = *(bool *)value; break;
  case SETTING_TYPE_INT: res->int_val = *(int *)value; break;
  case SETTING_TYPE_STRING: json_replace_string(&res->string_val, value); break;
  }
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
  LOG("Settings: Save");
  autofree char *json = json_print(settings);
  if (!write_string_to_file(settings_path, json)) LOG("Settings: Failed to save settings");
  last_save_time = TIME_NOW;
  pending_save = false;
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
