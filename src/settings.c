// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "settings.h"
#include "config.h"
#include "glib.h"

#include <libsecret/secret.h>

static void load_user(ErrandsSettings *self);
static void migrate_from_46(ErrandsSettings *self);
static void save(ErrandsSettings *self);

#define SECRET_SCHEMA ((SecretSchema){APP_ID, SECRET_SCHEMA_NONE, {{"account", SECRET_SCHEMA_ATTRIBUTE_STRING}, {"NULL", 0}}})

struct _ErrandsSettings {
  GObject parent_instance;

  gchar *settings_path;

  // Properties
  gboolean maximized;
  gboolean show_completed;
  gboolean show_cancelled;
  gboolean background;
  gboolean startup;
  gboolean sync_enabled;
  gint theme;
  gint sort_by;
  gint sort_order;
  gint sync_interval;
  gint window_width;
  gint window_height;
  gchar *last_list_uid;
  gchar *sync_url;
  gchar *sync_username;
  gchar *sync_password;
  GStrv tags;
};

G_DEFINE_TYPE(ErrandsSettings, errands_settings, G_TYPE_OBJECT)

enum {
  PROP_0,

  PROP_MAXIMIZED,
  PROP_SHOW_COMPLETED,
  PROP_SHOW_CANCELLED,
  PROP_BACKGROUND,
  PROP_STARTUP,
  PROP_SYNC,

  PROP_THEME,
  PROP_SORT_BY,
  PROP_SORT_ORDER,
  PROP_SYNC_INTERVAL,
  PROP_WINDOW_WIDTH,
  PROP_WINDOW_HEIGHT,

  PROP_LAST_LIST_UID,
  PROP_SYNC_URL,
  PROP_SYNC_USERNAME,
  PROP_SYNC_PASSWORD,

  PROP_TAGS,

  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsSettings *self = ERRANDS_SETTINGS(object);
  switch (prop_id) {
  case PROP_MAXIMIZED: g_value_set_boolean(value, self->maximized); break;
  case PROP_SHOW_COMPLETED: g_value_set_boolean(value, self->show_completed); break;
  case PROP_SHOW_CANCELLED: g_value_set_boolean(value, self->show_cancelled); break;
  case PROP_BACKGROUND: g_value_set_boolean(value, self->background); break;
  case PROP_STARTUP: g_value_set_boolean(value, self->startup); break;
  case PROP_SYNC: g_value_set_boolean(value, self->sync_enabled); break;

  case PROP_THEME: g_value_set_int(value, self->theme); break;
  case PROP_SORT_BY: g_value_set_int(value, self->sort_by); break;
  case PROP_SORT_ORDER: g_value_set_int(value, self->sort_order); break;
  case PROP_SYNC_INTERVAL: g_value_set_int(value, self->sync_interval); break;
  case PROP_WINDOW_WIDTH: g_value_set_int(value, self->window_width); break;
  case PROP_WINDOW_HEIGHT: g_value_set_int(value, self->window_height); break;

  case PROP_LAST_LIST_UID: g_value_set_string(value, self->last_list_uid); break;
  case PROP_SYNC_URL: g_value_set_string(value, self->sync_url); break;
  case PROP_SYNC_USERNAME: g_value_set_string(value, self->sync_username); break;
  case PROP_SYNC_PASSWORD: g_value_set_string(value, self->sync_password); break;

  case PROP_TAGS: g_value_set_pointer(value, self->tags); break;

  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void save(ErrandsSettings *self) {
  g_autoptr(GKeyFile) file = g_key_file_new();

  g_key_file_set_boolean(file, "app", "background", self->background);
  g_key_file_set_boolean(file, "app", "startup", self->startup);
  g_key_file_set_boolean(file, "window", "maximized", self->maximized);
  g_key_file_set_integer(file, "window", "theme", self->theme);
  g_key_file_set_integer(file, "window", "height", self->window_height);
  g_key_file_set_integer(file, "window", "width", self->window_width);
  g_key_file_set_integer(file, "task_list", "sort_by", self->sort_by);
  g_key_file_set_integer(file, "task_list", "sort_order", self->sort_order);
  g_key_file_set_boolean(file, "task_list", "show_cancelled", self->show_cancelled);
  g_key_file_set_boolean(file, "task_list", "show_completed", self->show_completed);
  g_key_file_set_string(file, "task_list", "last_list_uid", self->last_list_uid);
  g_key_file_set_string_list(file, "task_list", "tags", (const gchar *const *)self->tags,
                             self->tags ? g_strv_length(self->tags) : 0);
  g_key_file_set_boolean(file, "sync", "enabled", self->sync_enabled);
  g_key_file_set_integer(file, "sync", "interval", self->sync_interval);
  g_key_file_set_string(file, "sync", "url", self->sync_url);
  g_key_file_set_string(file, "sync", "username", self->sync_username);

  g_autoptr(GError) error = NULL;
  if (!g_key_file_save_to_file(file, self->settings_path, &error))
    g_warning("Settings: Failed to save settings: %s", error->message);
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsSettings *self = ERRANDS_SETTINGS(object);
  switch (prop_id) {
  case PROP_MAXIMIZED: self->maximized = g_value_get_boolean(value); break;
  case PROP_SHOW_COMPLETED: self->show_completed = g_value_get_boolean(value); break;
  case PROP_SHOW_CANCELLED: self->show_cancelled = g_value_get_boolean(value); break;
  case PROP_BACKGROUND: self->background = g_value_get_boolean(value); break;
  case PROP_STARTUP: self->startup = g_value_get_boolean(value); break;
  case PROP_SYNC: self->sync_enabled = g_value_get_boolean(value); break;

  case PROP_THEME: {
    self->theme = g_value_get_int(value);
    if (self->theme == 2) self->theme = 4;
  } break;
  case PROP_SORT_BY: self->sort_by = g_value_get_int(value); break;
  case PROP_SORT_ORDER: self->sort_order = g_value_get_int(value); break;
  case PROP_SYNC_INTERVAL: self->sync_interval = g_value_get_int(value); break;
  case PROP_WINDOW_WIDTH: self->window_width = g_value_get_int(value); break;
  case PROP_WINDOW_HEIGHT: self->window_height = g_value_get_int(value); break;

  case PROP_LAST_LIST_UID: {
    g_clear_pointer(&self->last_list_uid, g_free);
    const char *last_list_uid = g_value_get_string(value);
    self->last_list_uid = g_strdup(last_list_uid ? last_list_uid : "");
  } break;
  case PROP_SYNC_URL: {
    g_clear_pointer(&self->sync_url, g_free);
    const char *sync_url = g_value_get_string(value);
    self->sync_url = g_strdup(sync_url ? sync_url : "");
  } break;
  case PROP_SYNC_USERNAME: {
    g_clear_pointer(&self->sync_username, g_free);
    const char *sync_username = g_value_get_string(value);
    self->sync_username = g_strdup(sync_username ? sync_username : "");
  } break;
  case PROP_SYNC_PASSWORD: {
    const char *sync_password = g_value_get_string(value);
    secret_password_store_sync(&SECRET_SCHEMA, SECRET_COLLECTION_DEFAULT, "Errands CalDAV credentials",
                               sync_password ? sync_password : "", NULL, NULL, "account", "CalDAV", NULL);
  } break;
  case PROP_TAGS: {
    g_clear_pointer(&self->tags, g_strfreev);
    self->tags = g_value_get_pointer(value);
  } break;

  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
  g_idle_add(G_SOURCE_FUNC(save), self);
}

static void finalize(GObject *gobject) {
  ErrandsSettings *self = ERRANDS_SETTINGS(gobject);
  save(self);
  g_clear_pointer(&self->last_list_uid, g_free);
  g_clear_pointer(&self->sync_url, g_free);
  g_clear_pointer(&self->sync_username, g_free);
  g_clear_pointer(&self->settings_path, g_free);
  g_clear_pointer(&self->tags, g_strfreev);
  G_OBJECT_CLASS(errands_settings_parent_class)->finalize(gobject);
}

static void errands_settings_class_init(ErrandsSettingsClass *class) {
  GObjectClass *object_class = G_OBJECT_CLASS(class);
  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  obj_properties[PROP_MAXIMIZED] =
      g_param_spec_boolean("maximized", "Maximized", "Window is maximized", FALSE, G_PARAM_READWRITE);
  obj_properties[PROP_SHOW_COMPLETED] =
      g_param_spec_boolean("show-completed", "Show Completed", "Show completed tasks", FALSE, G_PARAM_READWRITE);
  obj_properties[PROP_SHOW_CANCELLED] =
      g_param_spec_boolean("show-cancelled", "Show Cancelled", "Show cancelled tasks", FALSE, G_PARAM_READWRITE);
  obj_properties[PROP_BACKGROUND] =
      g_param_spec_boolean("background", "Background", "Run in background", TRUE, G_PARAM_READWRITE);
  obj_properties[PROP_STARTUP] = g_param_spec_boolean("startup", "Startup", "Run on startup", TRUE, G_PARAM_READWRITE);
  obj_properties[PROP_SYNC] = g_param_spec_boolean("sync-enabled", "Sync", "Enable sync", FALSE, G_PARAM_READWRITE);

  obj_properties[PROP_THEME] = g_param_spec_int("theme", "Theme", "Theme", 0, 4, 0, G_PARAM_READWRITE);
  obj_properties[PROP_SORT_BY] =
      g_param_spec_int("sort-by", "Sort By", "Sort By", 0, 3, SORT_TYPE_CREATION_DATE, G_PARAM_READWRITE);
  obj_properties[PROP_SORT_ORDER] =
      g_param_spec_int("sort-order", "Sort Order", "Sort Order", 0, 1, SORT_ORDER_DESC, G_PARAM_READWRITE);
  obj_properties[PROP_SYNC_INTERVAL] =
      g_param_spec_int("sync-interval", "Sync Interval", "Sync Interval", 0, G_MAXINT, 30, G_PARAM_READWRITE);
  obj_properties[PROP_WINDOW_WIDTH] =
      g_param_spec_int("window-width", "Window Width", "Window Width", 0, G_MAXINT, 800, G_PARAM_READWRITE);
  obj_properties[PROP_WINDOW_HEIGHT] =
      g_param_spec_int("window-height", "Window Height", "Window Height", 0, G_MAXINT, 600, G_PARAM_READWRITE);

  obj_properties[PROP_LAST_LIST_UID] =
      g_param_spec_string("last-list-uid", "Last List UID", "Last List UID", "", G_PARAM_READWRITE);
  obj_properties[PROP_SYNC_URL] = g_param_spec_string("sync-url", "Sync URL", "Sync URL", "", G_PARAM_READWRITE);
  obj_properties[PROP_SYNC_USERNAME] =
      g_param_spec_string("sync-username", "Sync Username", "Sync Username", "", G_PARAM_READWRITE);
  obj_properties[PROP_SYNC_PASSWORD] =
      g_param_spec_string("sync-password", "Sync Password", "Sync Password", "", G_PARAM_READWRITE);

  obj_properties[PROP_TAGS] = g_param_spec_pointer("tags", "Tags", "Tags", G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void errands_settings_init(ErrandsSettings *self) {
  g_message("Settings: Initialize");

  // Setup default values
  self->maximized = FALSE;
  self->show_completed = FALSE;
  self->show_cancelled = FALSE;
  self->background = TRUE;
  self->startup = TRUE;
  self->sync_enabled = FALSE;
  self->theme = 0;
  self->sort_by = 0;
  self->sort_order = 0;
  self->sync_interval = 30;
  self->window_width = 1280;
  self->window_height = 720;
  self->last_list_uid = g_strdup("");
  self->sync_url = g_strdup("");
  self->sync_username = g_strdup("");
  self->sync_password = secret_password_lookup_sync(&SECRET_SCHEMA, NULL, NULL, "account", "CalDAV", NULL);

  self->settings_path = g_build_filename(g_get_user_data_dir(), "errands", "settings.ini", NULL);
  if (g_file_test(self->settings_path, G_FILE_TEST_EXISTS)) load_user(self);
  else {
    migrate_from_46(self);
    save(self);
  }
}

// --- PRIVATE --- //

static void load_user(ErrandsSettings *self) {
  g_message("Settings: Loading user configuration '%s'", self->settings_path);
  g_autoptr(GKeyFile) file = g_key_file_new();
  g_autoptr(GError) error = NULL;
  if (!g_key_file_load_from_file(file, self->settings_path, G_KEY_FILE_NONE, &error)) {
    g_warning("Settings: Failed to load user configuration: %s", error->message);
    return;
  }
  self->maximized = g_key_file_get_boolean(file, "window", "maximized", NULL);
  self->show_completed = g_key_file_get_boolean(file, "task_list", "show_completed", NULL);
  self->show_cancelled = g_key_file_get_boolean(file, "task_list", "show_cancelled", NULL);
  self->background = g_key_file_get_boolean(file, "app", "background", NULL);
  self->startup = g_key_file_get_boolean(file, "app", "startup", NULL);
  self->sync_enabled = g_key_file_get_boolean(file, "sync", "enabled", NULL);

  self->theme = g_key_file_get_integer(file, "window", "theme", NULL);
  self->sort_by = g_key_file_get_integer(file, "task_list", "sort_by", NULL);
  self->sort_order = g_key_file_get_integer(file, "task_list", "sort_order", NULL);
  self->sync_interval = g_key_file_get_integer(file, "sync", "interval", NULL);
  self->window_width = g_key_file_get_integer(file, "window", "width", NULL);
  self->window_height = g_key_file_get_integer(file, "window", "height", NULL);

  self->last_list_uid = g_key_file_get_string(file, "task_list", "last_list_uid", NULL);
  self->sync_url = g_key_file_get_string(file, "sync", "url", NULL);
  self->sync_username = g_key_file_get_string(file, "sync", "username", NULL);
  self->sync_password = secret_password_lookup_sync(&SECRET_SCHEMA, NULL, NULL, "account", "CalDAV", NULL);

  self->tags = g_key_file_get_string_list(file, "task_list", "tags", NULL, NULL);
}

static void migrate_from_46(ErrandsSettings *self) {
  g_autofree gchar *password = secret_password_lookup_sync(&SECRET_SCHEMA, NULL, NULL, "account", "Nextcloud", NULL);
  if (!password) return;
  errands_settings_set_sync_password(self, password);
}

// --- PUBLIC --- //

ErrandsSettings *errands_settings_new() { return g_object_new(ERRANDS_TYPE_SETTINGS, NULL); }

void errands_settings_add_tag(ErrandsSettings *self, const gchar *tag) {
  if (!g_strv_contains((const char *const *)self->tags, tag)) {
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    g_strv_builder_addv(builder, (const char **)self->tags);
    g_strv_builder_add(builder, tag);
    GStrv new_tags = g_strv_builder_end(builder);
    errands_settings_set_tags(self, new_tags);
  }
}

void errands_settings_remove_tag(ErrandsSettings *self, const gchar *tag) {
  if (g_strv_contains((const char *const *)self->tags, tag)) {
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    for (gint i = 0; self->tags[i]; i++)
      if (!g_str_equal(self->tags[i], tag)) g_strv_builder_add(builder, self->tags[i]);
    GStrv new_tags = g_strv_builder_end(builder);
    errands_settings_set_tags(self, new_tags);
  }
}

// --- GETTERS --- //

gboolean errands_settings_get_maximized(ErrandsSettings *self) { return self->maximized; }
gboolean errands_settings_get_show_completed(ErrandsSettings *self) { return self->show_completed; }
gboolean errands_settings_get_show_cancelled(ErrandsSettings *self) { return self->show_cancelled; }
gboolean errands_settings_get_background(ErrandsSettings *self) { return self->background; }
gboolean errands_settings_get_startup(ErrandsSettings *self) { return self->startup; }
gboolean errands_settings_get_sync_enabled(ErrandsSettings *self) { return self->sync_enabled; }

gint errands_settings_get_theme(ErrandsSettings *self) { return self->theme; }
gint errands_settings_get_sort_by(ErrandsSettings *self) { return self->sort_by; }
gint errands_settings_get_sort_order(ErrandsSettings *self) { return self->sort_order; }
gint errands_settings_get_sync_interval(ErrandsSettings *self) { return self->sync_interval; }
gint errands_settings_get_window_width(ErrandsSettings *self) { return self->window_width; }
gint errands_settings_get_window_height(ErrandsSettings *self) { return self->window_height; }

const gchar *errands_settings_get_last_list_uid(ErrandsSettings *self) { return self->last_list_uid; }
const gchar *errands_settings_get_sync_url(ErrandsSettings *self) { return self->sync_url; }
const gchar *errands_settings_get_sync_username(ErrandsSettings *self) { return self->sync_username; }
const gchar *errands_settings_get_sync_password(ErrandsSettings *self) { return self->sync_password; }

const GStrv errands_settings_get_tags(ErrandsSettings *self) { return self->tags; }

// --- SETTERS --- //

void errands_settings_set_maximized(ErrandsSettings *self, gboolean maximized) {
  g_object_set(self, "maximized", maximized, NULL);
}
void errands_settings_set_show_completed(ErrandsSettings *self, gboolean show_completed) {
  g_object_set(self, "show-completed", show_completed, NULL);
}
void errands_settings_set_show_cancelled(ErrandsSettings *self, gboolean show_cancelled) {
  g_object_set(self, "show-cancelled", show_cancelled, NULL);
}
void errands_settings_set_background(ErrandsSettings *self, gboolean background) {
  g_object_set(self, "background", background, NULL);
}
void errands_settings_set_startup(ErrandsSettings *self, gboolean startup) { g_object_set(self, "startup", startup, NULL); }
void errands_settings_set_sync_enabled(ErrandsSettings *self, gboolean sync_enabled) {
  g_object_set(self, "sync-enabled", sync_enabled, NULL);
}

void errands_settings_set_theme(ErrandsSettings *self, gint theme) { g_object_set(self, "theme", theme, NULL); }
void errands_settings_set_sort_by(ErrandsSettings *self, gint sort_by) { g_object_set(self, "sort-by", sort_by, NULL); }
void errands_settings_set_sort_order(ErrandsSettings *self, gint sort_order) {
  g_object_set(self, "sort-order", sort_order, NULL);
}
void errands_settings_set_sync_interval(ErrandsSettings *self, gint sync_interval) {
  g_object_set(self, "sync-interval", sync_interval, NULL);
}
void errands_settings_set_window_width(ErrandsSettings *self, gint window_width) {
  g_object_set(self, "window-width", window_width, NULL);
}
void errands_settings_set_window_height(ErrandsSettings *self, gint window_height) {
  g_object_set(self, "window-height", window_height, NULL);
}

void errands_settings_set_last_list_uid(ErrandsSettings *self, const gchar *last_list_uid) {
  g_object_set(self, "last-list-uid", last_list_uid, NULL);
}
void errands_settings_set_sync_provider(ErrandsSettings *self, const gchar *sync_provider) {
  g_object_set(self, "sync-provider", sync_provider, NULL);
}
void errands_settings_set_sync_url(ErrandsSettings *self, const gchar *sync_url) {
  g_object_set(self, "sync-url", sync_url, NULL);
}
void errands_settings_set_sync_username(ErrandsSettings *self, const gchar *sync_username) {
  g_object_set(self, "sync-username", sync_username, NULL);
}
void errands_settings_set_sync_password(ErrandsSettings *self, const gchar *sync_password) {
  g_object_set(self, "sync-password", sync_password, NULL);
}
void errands_settings_set_tags(ErrandsSettings *self, GStrv tags) { g_object_set(self, "tags", tags, NULL); }
