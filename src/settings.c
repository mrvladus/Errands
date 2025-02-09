#include "settings.h"
#include "glib.h"
#include "utils.h"

#include <json-glib/json-glib.h>

// Save settings with cooldown period of 1s.
static void errands_settings_save();

// --- GLOBAL SETTINGS VARIABLES --- //

static const char *settings_path;
static time_t last_save_time = 0;
static bool pending_save = false;
static JsonGenerator *generator = NULL;
static JsonNode *settings_root = NULL;

// --- LOADING --- //

void errands_settings_load_default() {
  LOG("Settings: Load default configuration");

  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);

  json_builder_set_member_name(builder, "show_completed");
  json_builder_add_boolean_value(builder, TRUE);

  json_builder_set_member_name(builder, "window_width");
  json_builder_add_int_value(builder, 800);

  json_builder_set_member_name(builder, "window_height");
  json_builder_add_int_value(builder, 600);

  json_builder_set_member_name(builder, "maximized");
  json_builder_add_boolean_value(builder, FALSE);

  json_builder_set_member_name(builder, "last_list_uid");
  json_builder_add_string_value(builder, "");

  json_builder_set_member_name(builder, "sort_by");
  json_builder_add_string_value(builder, "default");

  json_builder_set_member_name(builder, "sync");
  json_builder_add_boolean_value(builder, FALSE);

  json_builder_end_object(builder);

  settings_root = json_builder_get_root(builder);
  g_object_unref(builder);
}

void errands_settings_load_user() {
  LOG("Settings: Load user configuration");
  g_autoptr(GError) error = NULL;
  g_autoptr(JsonParser) parser = json_parser_new();

  if (!json_parser_load_from_file(parser, settings_path, &error)) {
    LOG("Settings: Failed to parse settings: %s", error->message);
    return;
  }

  JsonNode *user_root = json_parser_get_root(parser);
  if (JSON_NODE_HOLDS_OBJECT(user_root)) {
    JsonObject *user_obj = json_node_get_object(user_root);
    JsonObject *settings_obj = json_node_get_object(settings_root);

    const gchar *const settings_keys[] = {"show_completed", "window_width", "window_height", "maximized",
                                          "last_list_uid",  "sort_by",      "sync"};

    for (guint i = 0; i < G_N_ELEMENTS(settings_keys); i++) {
      const gchar *key = settings_keys[i];
      if (json_object_has_member(user_obj, key)) {
        JsonNode *val = json_object_get_member(user_obj, key);
        json_object_set_member(settings_obj, key, json_node_copy(val));
      }
    }
  }
}

// Migrate from GSettings to settings.json
void errands_settings_migrate() { LOG("Settings: Migrate to settings.json"); }

void errands_settings_init() {
  LOG("Settings: Initialize");
  settings_path = g_build_filename(g_get_user_data_dir(), "errands", "settings.json", NULL);
  errands_settings_load_default();
  if (g_file_test(settings_path, G_FILE_TEST_EXISTS)) {
    errands_settings_load_user();
  } else {
    errands_settings_migrate();
  }
  errands_settings_save();
}

// --- GET / SET SETTINGS --- //

ErrandsSetting errands_settings_get(const char *key, ErrandsSettingType type) {
  ErrandsSetting setting = {0};
  JsonObject *settings_obj = json_node_get_object(settings_root);
  JsonNode *value = json_object_get_member(settings_obj, key);
  if (type == SETTING_TYPE_INT && JSON_NODE_HOLDS_VALUE(value))
    setting.i = json_node_get_int(value);
  else if (type == SETTING_TYPE_BOOL && JSON_NODE_HOLDS_VALUE(value))
    setting.b = json_node_get_boolean(value);
  else if (type == SETTING_TYPE_STRING && JSON_NODE_HOLDS_VALUE(value))
    setting.s = json_node_get_string(value);
  return setting;
}

void errands_settings_set(const char *key, ErrandsSettingType type, void *value) {
  JsonObject *settings_obj = json_node_get_object(settings_root);
  if (type == SETTING_TYPE_INT)
    json_object_set_int_member(settings_obj, key, *(int *)value);
  else if (type == SETTING_TYPE_BOOL)
    json_object_set_boolean_member(settings_obj, key, *(bool *)value);
  else if (type == SETTING_TYPE_STRING)
    json_object_set_string_member(settings_obj, key, (const char *)value);
  errands_settings_save();
}

// --- SAVING SETTINGS --- //

static void perform_save() {
  LOG("Settings: Save");
  if (!generator) {
    generator = json_generator_new();
    json_generator_set_pretty(generator, FALSE);
  }
  json_generator_set_root(generator, settings_root);
  g_autofree gchar *json_string = json_generator_to_data(generator, NULL);
  g_autoptr(GError) error = NULL;
  if (!g_file_set_contents(settings_path, json_string, -1, &error))
    LOG("Settings: Failed to save settings: %s", error->message);
  last_save_time = time(NULL);
  pending_save = false;
}

void errands_settings_save() {
  double diff = difftime(time(NULL), last_save_time);
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
