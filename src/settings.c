#include "settings.h"
#include "external/cJSON.h"
#include "utils.h"

#include <glib.h>
#include <stdlib.h>
#include <string.h>

// Save settings with cooldown period of 1s.
static void errands_settings_save();

// --- GLOBAL SETTINGS VARIABLES --- //

static const char *settings_path;
static const char *default_settings =
    "{\"show_completed\":true,\"window_width\":800,\"window_height\":600,\"maximized\":false}";
static time_t last_save_time = 0;
static bool pending_save = false;
static cJSON *settings;

// Migrate from GSettings to settings.json
bool errands_settings_migrate() {
  LOG("Settings: Migrate to settings.json");
  return false;
}

void errands_settings_init() {
  LOG("Settings: Initialize");
  settings_path = g_build_path("/", g_get_user_data_dir(), "errands", "settings.json", NULL);
  // Create settings file
  if (!file_exists(settings_path)) {
    bool migrated = errands_settings_migrate();
    if (!migrated) {
      LOG("Settings: Create default");
      write_string_to_file(settings_path, default_settings);
    }
    settings = cJSON_Parse(default_settings);
  } else {
    LOG("Settings: Read");
    char *settings_str = read_file_to_string(settings_path);
    if (!settings_str || !strcmp(settings_str, "")) {
      LOG("Settings: settings.json is corrupted. Creating default.");
      settings = cJSON_Parse(default_settings);
      write_string_to_file(settings_path, default_settings);
      free(settings_str);
      return;
    }
    settings = cJSON_Parse(settings_str);
    free(settings_str);
  }
}

int errands_settings_get_int(const char *key) {
  return cJSON_GetObjectItem(settings, key)->valueint;
}

bool errands_settings_get_bool(const char *key) {
  return cJSON_GetObjectItem(settings, key)->valueint;
}

char *errands_settings_get_str(const char *key) {
  return strdup(cJSON_GetObjectItem(settings, key)->valuestring);
}

void errands_settings_set(const char *key, const char *type, void *value) {
  cJSON *val = cJSON_GetObjectItem(settings, key);
  if (!strcmp(type, "int"))
    cJSON_SetIntValue(val, *(int *)value);
  else if (!strcmp(type, "bool"))
    cJSON_SetBoolValue(val, *(bool *)value);
  else if (!strcmp(type, "string"))
    cJSON_SetValuestring(val, (const char *)value);
  errands_settings_save();
}

// --- SAVING SETTINGS --- //

static void perform_save() {
  LOG("Settings: Save");
  char *json_string = cJSON_PrintUnformatted(settings);
  write_string_to_file(settings_path, json_string);
  free(json_string);
  last_save_time = time(NULL);
  pending_save = false;
}

void errands_settings_save() {
  float diff = difftime(time(NULL), last_save_time);
  if (pending_save)
    return;
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
