#include "settings.h"
#include "utils.h"
#include "vendor/cJSON.h"

// Save settings with cooldown period of 1s.
static void errands_settings_save();

// --- GLOBAL SETTINGS VARIABLES --- //

static const char *settings_path;
static time_t last_save_time = 0;
static bool pending_save = false;

static cJSON *settings = NULL;

// --- LOADING --- //

void errands_settings_load_default() {
  LOG("Settings: Load default configuration");
  settings = cJSON_CreateObject();
  cJSON_AddBoolToObject(settings, "show_completed", true);
  cJSON_AddNumberToObject(settings, "window_width", 800);
  cJSON_AddNumberToObject(settings, "window_height", 600);
  cJSON_AddBoolToObject(settings, "maximized", true);
  cJSON_AddStringToObject(settings, "last_list_uid", "");
  cJSON_AddStringToObject(settings, "sort_by", "default");
  cJSON_AddBoolToObject(settings, "sync", false);
  cJSON_AddStringToObject(settings, "sync_provider", "caldav");
  cJSON_AddStringToObject(settings, "sync_url", "");
  cJSON_AddStringToObject(settings, "sync_username", "");
  cJSON_AddStringToObject(settings, "tags", "");
}

void errands_settings_load_user() {
  LOG("Settings: Load user configuration");
  char *json = read_file_to_string(settings_path);
  if (!json) return;
  cJSON *json_parsed = cJSON_Parse(json);
  if (!json_parsed) return;
  const char *const settings_keys[] = {"show_completed", "window_width", "window_height", "maximized",
                                       "last_list_uid",  "sort_by",      "sync",          "sync_provider",
                                       "sync_url",       "sync_user",    "tags"};
  for (size_t i = 0; i < G_N_ELEMENTS(settings_keys); i++)
    cJSON_ReplaceItemInObject(settings, settings_keys[i], cJSON_DetachItemFromObject(json_parsed, settings_keys[i]));
  cJSON_Delete(json_parsed);
  free(json);
}

// TODO: Migrate from GSettings to settings.json
void errands_settings_migrate() { LOG("Settings: Migrate to settings.json"); }

void errands_settings_init() {
  LOG("Settings: Initialize");
  settings_path = g_build_filename(g_get_user_data_dir(), "errands", "settings.json", NULL);
  errands_settings_load_default();
  if (g_file_test(settings_path, G_FILE_TEST_EXISTS)) errands_settings_load_user();
  else errands_settings_migrate();
  errands_settings_save();
}

// --- GET / SET SETTINGS --- //

ErrandsSetting errands_settings_get(const char *key, ErrandsSettingType type) {
  ErrandsSetting setting = {0};
  cJSON *res = cJSON_GetObjectItem(settings, key);
  if (type == SETTING_TYPE_INT && cJSON_IsNumber(res)) setting.i = res->valueint;
  else if (type == SETTING_TYPE_BOOL && cJSON_IsBool(res)) setting.b = res->valueint;
  else if (type == SETTING_TYPE_STRING && cJSON_IsString(res)) setting.s = res->valuestring;
  return setting;
}

void errands_settings_set(const char *key, ErrandsSettingType type, void *value) {
  cJSON *res = cJSON_GetObjectItem(settings, key);
  if (type == SETTING_TYPE_INT) cJSON_SetIntValue(res, *(int *)value);
  else if (type == SETTING_TYPE_BOOL) cJSON_SetBoolValue(res, *(bool *)value);
  else if (type == SETTING_TYPE_STRING) cJSON_SetValuestring(res, (const char *)value);
  errands_settings_save();
}

// Global tags

GStrv errands_settings_get_tags() {
  cJSON *res = cJSON_GetObjectItem(settings, "tags");
  return g_strsplit(res->valuestring, ",", -1);
}

void errands_settings_set_tags(GStrv tags) {
  g_autofree char *value = g_strjoinv(",", tags);
  cJSON_SetValuestring(cJSON_GetObjectItem(settings, "tags"), value);
  errands_settings_save();
}

void errands_settings_add_tag(const char *tag) {
  cJSON *res = cJSON_GetObjectItem(settings, "tags");
  g_auto(GStrv) tags = g_strsplit(res->valuestring, ",", -1);
  if (!g_strv_contains((const char *const *)tags, tag)) {
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    g_strv_builder_addv(builder, (const char **)tags);
    g_strv_builder_add(builder, tag);
    g_auto(GStrv) new_tags = g_strv_builder_end(builder);
    errands_settings_set_tags(new_tags);
  }
}

void errands_settings_remove_tag(const char *tag) {
  cJSON *res = cJSON_GetObjectItem(settings, "tags");
  g_auto(GStrv) tags = g_strsplit(res->valuestring, ",", -1);
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
  g_autoptr(GError) error = NULL;
  char *json = cJSON_Print(settings);
  if (!g_file_set_contents(settings_path, json, -1, &error))
    LOG("Settings: Failed to save settings: %s", error->message);
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
