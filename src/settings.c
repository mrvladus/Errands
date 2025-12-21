#include "settings.h"

#define JSON_H_IMPLEMENTATION
#include "vendor/json.h"
#include "vendor/toolbox.h"

#include <libsecret/secret.h>

AUTOPTR_DEFINE(JSON, json_free)

// Save settings with cooldown period of 1s.
static void errands__settings_save();

// --- GLOBAL VARIABLES --- //

static char *settings_path;
static time_t last_save_time = 0;
static bool pending_save = false;
static JSON *settings = NULL;
static const SecretSchema secret_schema = {
    APP_ID,
    SECRET_SCHEMA_NONE,
    {
        {"account", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {"NULL", 0},
    },
};

#define SETTING_KEY_STR(ErrandsSettingsKey, key_str) [ErrandsSettingsKey] = key_str

static const char *const settings_keys_strs[] = {
    SETTING_KEY_STR(SETTING_LAST_LIST_UID, "last_list_uid"),
    SETTING_KEY_STR(SETTING_MAXIMIZED, "maximized"),
    SETTING_KEY_STR(SETTING_SHOW_CANCELLED, "show_cancelled"),
    SETTING_KEY_STR(SETTING_SHOW_COMPLETED, "show_completed"),
    SETTING_KEY_STR(SETTING_SORT_BY, "sort_by"),
    SETTING_KEY_STR(SETTING_SORT_ORDER, "sort_order"),
    SETTING_KEY_STR(SETTING_SYNC, "sync"),
    SETTING_KEY_STR(SETTING_SYNC_PROVIDER, "sync_provider"),
    SETTING_KEY_STR(SETTING_SYNC_URL, "sync_url"),
    SETTING_KEY_STR(SETTING_SYNC_USERNAME, "sync_username"),
    SETTING_KEY_STR(SETTING_TAGS, "tags"),
    SETTING_KEY_STR(SETTING_WINDOW_HEIGHT, "window_height"),
    SETTING_KEY_STR(SETTING_WINDOW_WIDTH, "window_width"),
    SETTING_KEY_STR(SETTING_NOTIFICATIONS, "notifications"),
    SETTING_KEY_STR(SETTING_BACKGROUND, "background"),
    SETTING_KEY_STR(SETTING_STARTUP, "startup"),
    SETTING_KEY_STR(SETTING_THEME, "theme"),
};

#define SETTING(key) settings_keys_strs[key]

// --- LOADING --- //

void errands_settings_load_default() {
#define SETTING_ADD(ErrandsSettingsKey, json_type, value)                                                              \
  json_object_add(settings, SETTING(ErrandsSettingsKey), json_##json_type##_new(value));

  settings = json_object_new();

  SETTING_ADD(SETTING_MAXIMIZED, bool, false);
  SETTING_ADD(SETTING_SHOW_CANCELLED, bool, true);
  SETTING_ADD(SETTING_SHOW_COMPLETED, bool, true);
  SETTING_ADD(SETTING_SYNC, bool, false);
  SETTING_ADD(SETTING_BACKGROUND, bool, true);
  SETTING_ADD(SETTING_NOTIFICATIONS, bool, true);
  SETTING_ADD(SETTING_STARTUP, bool, false);

  SETTING_ADD(SETTING_THEME, int, SETTING_THEME_SYSTEM);
  SETTING_ADD(SETTING_WINDOW_HEIGHT, int, 600);
  SETTING_ADD(SETTING_WINDOW_WIDTH, int, 800);
  SETTING_ADD(SETTING_SORT_BY, int, SORT_TYPE_CREATION_DATE);
  SETTING_ADD(SETTING_SORT_ORDER, int, SORT_ORDER_DESC);

  SETTING_ADD(SETTING_LAST_LIST_UID, string, "");
  SETTING_ADD(SETTING_SYNC_PROVIDER, string, "caldav");
  SETTING_ADD(SETTING_SYNC_URL, string, "");
  SETTING_ADD(SETTING_SYNC_USERNAME, string, "");
  SETTING_ADD(SETTING_TAGS, string, "");

  LOG("Settings: Created default configuration");
}

void errands_settings_load_user() {
  LOG("Settings: Load user configuration");
  autofree char *json = read_file_to_string(settings_path);
  if (!json) return;
  autoptr(JSON) json_parsed = json_parse(json);
  if (!json_parsed) return;
  for (size_t i = 0; i < STATIC_ARRAY_SIZE(settings_keys_strs); i++) {
    const char *key = settings_keys_strs[i];
    JSON *node = json_object_get(json_parsed, key);
    if (!node) continue;
    json_object_add(settings, key, json_dup(node));
  }
}

static void errands_settings_migrate_from_46() {
  g_autofree gchar *password = secret_password_lookup_sync(&secret_schema, NULL, NULL, "account", "Nextcloud", NULL);
  if (!password) return;
  errands_settings_set_password(password);
  // TODO: Migrate from GSettings to settings.json
}

void errands_settings_init() {
  LOG("Settings: Initialize");
  settings_path = g_build_filename(g_get_user_data_dir(), "errands", "settings.json", NULL);
  errands_settings_load_default();
  if (file_exists(settings_path)) errands_settings_load_user();
  else errands_settings_migrate_from_46();
  errands__settings_save();
}

void errands_settings_cleanup() {
  LOG("Settings: Cleanup");
  if (settings_path) g_free(settings_path);
  if (settings) json_free(settings);
}

// --- GET / SET SETTINGS --- //

ErrandsSetting errands_settings_get(ErrandsSettingsKey key) {
#define SETTING_GET_STR  out.s = res->string_val
#define SETTING_GET_INT  out.i = res->int_val
#define SETTING_GET_BOOL out.b = res->bool_val

  const JSON *res = json_object_get(settings, SETTING(key));
  ErrandsSetting out = {0};
  switch (key) {
  case SETTING_BACKGROUND: SETTING_GET_BOOL; break;
  case SETTING_MAXIMIZED: SETTING_GET_BOOL; break;
  case SETTING_NOTIFICATIONS: SETTING_GET_BOOL; break;
  case SETTING_SHOW_CANCELLED: SETTING_GET_BOOL; break;
  case SETTING_SHOW_COMPLETED: SETTING_GET_BOOL; break;
  case SETTING_STARTUP: SETTING_GET_BOOL; break;
  case SETTING_SYNC: SETTING_GET_BOOL; break;

  case SETTING_SORT_BY: SETTING_GET_INT; break;
  case SETTING_SORT_ORDER: SETTING_GET_INT; break;
  case SETTING_THEME: SETTING_GET_INT; break;
  case SETTING_WINDOW_HEIGHT: SETTING_GET_INT; break;
  case SETTING_WINDOW_WIDTH: SETTING_GET_INT; break;

  case SETTING_LAST_LIST_UID: SETTING_GET_STR; break;
  case SETTING_SYNC_PROVIDER: SETTING_GET_STR; break;
  case SETTING_SYNC_URL: SETTING_GET_STR; break;
  case SETTING_SYNC_USERNAME: SETTING_GET_STR; break;
  case SETTING_TAGS: SETTING_GET_STR; break;
  }
  return out;
}

void errands_settings_set(ErrandsSettingsKey key, void *value) {
#define SETTING_SET_STR  json_replace_string(&res->string_val, value)
#define SETTING_SET_INT  res->int_val = *(int *)value
#define SETTING_SET_BOOL res->bool_val = *(bool *)value

  JSON *res = json_object_get(settings, SETTING(key));
  switch (key) {
  case SETTING_MAXIMIZED: SETTING_SET_BOOL; break;
  case SETTING_SHOW_CANCELLED: SETTING_SET_BOOL; break;
  case SETTING_SHOW_COMPLETED: SETTING_SET_BOOL; break;
  case SETTING_SYNC: SETTING_SET_BOOL; break;
  case SETTING_NOTIFICATIONS: SETTING_SET_BOOL; break;
  case SETTING_BACKGROUND: SETTING_SET_BOOL; break;
  case SETTING_STARTUP: SETTING_SET_BOOL; break;

  case SETTING_SORT_BY: SETTING_SET_INT; break;
  case SETTING_SORT_ORDER: SETTING_SET_INT; break;
  case SETTING_WINDOW_HEIGHT: SETTING_SET_INT; break;
  case SETTING_WINDOW_WIDTH: SETTING_SET_INT; break;
  case SETTING_THEME: SETTING_SET_INT; break;

  case SETTING_LAST_LIST_UID: SETTING_SET_STR; break;
  case SETTING_SYNC_PROVIDER: SETTING_SET_STR; break;
  case SETTING_SYNC_URL: SETTING_SET_STR; break;
  case SETTING_SYNC_USERNAME: SETTING_SET_STR; break;
  case SETTING_TAGS: SETTING_SET_STR; break;
  }
  errands__settings_save();
}

// --- TAGS --- //

GStrv errands_settings_get_tags() {
  JSON *res = json_object_get(settings, "tags");
  return g_strsplit(res->string_val, ",", -1);
}

void errands_settings_set_tags(GStrv tags) {
  g_autofree char *value = g_strjoinv(",", tags);
  JSON *res = json_object_get(settings, "tags");
  json_replace_string(&res->string_val, value);
  errands__settings_save();
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

static void errands__perform_save() {
  autofree char *json = json_print(settings);
  if (!write_string_to_file(settings_path, json)) LOG("Settings: Failed to save settings");
  last_save_time = TIME_NOW;
  pending_save = false;
}

static void errands__settings_save() {
  double diff = difftime(TIME_NOW, last_save_time);
  if (pending_save) return;
  if (diff < 1.0f) {
    pending_save = true;
    g_timeout_add_seconds_once(1, (GSourceOnceFunc)errands__perform_save, NULL);
    return;
  } else {
    pending_save = true;
    errands__perform_save();
    return;
  }
}

// --- PASSWORDS --- //

gchar *errands_settings_get_password() {
  return secret_password_lookup_sync(&secret_schema, NULL, NULL, "account", "CalDAV", NULL);
}

void errands_settings_set_password(const char *password) {
  secret_password_store_sync(&secret_schema, SECRET_COLLECTION_DEFAULT, "Errands CalDAV credentials", password, NULL,
                             NULL, "account", "CalDAV", NULL);
}
