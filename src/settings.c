#include "settings.h"
#include "external/cJSON.h"
#include "glib.h"
#include "state.h"
#include "utils.h"

#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGHT 600

void errands_settings_init() {
  LOG("Initialize settings.json");

  char *settings_file_path =
      g_build_path("/", g_get_user_data_dir(), "errands", "settings.json", NULL);
  if (!file_exists(settings_file_path)) {
    errands_settings_migrate();

    LOG("Create default settings.json");

    char default_settings_format[] = "{\"show_completed\":true,"
                                     "\"window_width\":%d,"
                                     "\"window_height\":%d,"
                                     "\"maximized\":false}";
    str default_settings = str_new_printf(default_settings_format, DEFAULT_WIDTH, DEFAULT_HEIGHT);
    FILE *file = fopen(settings_file_path, "w");
    fprintf(file, "%s", default_settings.str);
    fclose(file);
    str_free(&default_settings);
  } else {
    LOG("Reading settings.json");

    char *settings_str = read_file_to_string(settings_file_path);
    if (settings_str) {
      cJSON *json = cJSON_Parse(settings_str);
      if (json) {
        cJSON *show_completed = cJSON_GetObjectItem(json, "show_completed");
        state.settings.show_completed = show_completed ? (bool)show_completed->valueint : true;

        cJSON *window_width = cJSON_GetObjectItem(json, "window_width");
        state.settings.window_width = window_width ? window_width->valueint : DEFAULT_WIDTH;

        cJSON *window_height = cJSON_GetObjectItem(json, "window_height");
        state.settings.window_height = window_height ? window_height->valueint : DEFAULT_HEIGHT;

        cJSON *maximized = cJSON_GetObjectItem(json, "maximized");
        state.settings.maximized = maximized ? (bool)maximized->valueint : false;
      }
      free(settings_str);
      cJSON_Delete(json);
    }
  }
  g_free(settings_file_path);
}

void errands_settings_migrate() { LOG("Migrate to settings.json"); }

static time_t last_save_time = 0;
static bool pending_save = false;

static void perform_save() {
  LOG("Settings: Save");

  cJSON *json = cJSON_CreateObject();

  cJSON_AddItemToObject(json, "show_completed", cJSON_CreateBool(state.settings.show_completed));
  cJSON_AddItemToObject(json, "window_width", cJSON_CreateNumber(state.settings.window_width));
  cJSON_AddItemToObject(json, "window_height", cJSON_CreateNumber(state.settings.window_height));
  cJSON_AddItemToObject(json, "maximized", cJSON_CreateBool(state.settings.maximized));

  // Save to file
  char *json_string = cJSON_PrintUnformatted(json);
  char *settings_file_path =
      g_build_path("/", g_get_user_data_dir(), "errands", "settings.json", NULL);

  FILE *file = fopen(settings_file_path, "w");
  if (file == NULL) {
    LOG("Error opening settings.json file");
    cJSON_Delete(json);
    free(json_string);
  }
  fprintf(file, "%s", json_string);
  fclose(file);

  // Clean up
  cJSON_Delete(json);
  free(json_string);
  g_free(settings_file_path);

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
