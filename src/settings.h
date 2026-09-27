#pragma once

#include <glib-object.h>

typedef enum {
  SORT_TYPE_CREATION_DATE,
  SORT_TYPE_START_DATE,
  SORT_TYPE_DUE_DATE,
  SORT_TYPE_PRIORITY,
} ErrandsSettingsSortType;

typedef enum {
  SORT_ORDER_DESC,
  SORT_ORDER_ASC,
} ErrandsSettingsSortOrder;

typedef enum {
  SETTING_THEME_SYSTEM,
  SETTING_THEME_LIGHT,
  SETTING_THEME_DARK,
} ErrandsSettingsTheme;

#define ERRANDS_TYPE_SETTINGS errands_settings_get_type()
G_DECLARE_FINAL_TYPE(ErrandsSettings, errands_settings, ERRANDS, SETTINGS, GObject)

ErrandsSettings *errands_settings_new();

// --- METHODS --- //

void errands_settings_add_tag(ErrandsSettings *self, const gchar *tag);
void errands_settings_remove_tag(ErrandsSettings *self, const gchar *tag);

// --- GETTERS --- //

gboolean errands_settings_get_maximized(ErrandsSettings *self);
gboolean errands_settings_get_show_completed(ErrandsSettings *self);
gboolean errands_settings_get_show_cancelled(ErrandsSettings *self);
gboolean errands_settings_get_background(ErrandsSettings *self);
gboolean errands_settings_get_startup(ErrandsSettings *self);
gboolean errands_settings_get_sync_enabled(ErrandsSettings *self);
gint errands_settings_get_theme(ErrandsSettings *self);
gint errands_settings_get_sort_by(ErrandsSettings *self);
gint errands_settings_get_sort_order(ErrandsSettings *self);
gint errands_settings_get_sync_interval(ErrandsSettings *self);
gint errands_settings_get_window_width(ErrandsSettings *self);
gint errands_settings_get_window_height(ErrandsSettings *self);
const gchar *errands_settings_get_last_list_uid(ErrandsSettings *self);
const gchar *errands_settings_get_sync_provider(ErrandsSettings *self);
const gchar *errands_settings_get_sync_url(ErrandsSettings *self);
const gchar *errands_settings_get_sync_username(ErrandsSettings *self);
const gchar *errands_settings_get_sync_password(ErrandsSettings *self);
const GStrv errands_settings_get_tags(ErrandsSettings *self);

// --- SETTERS --- //

void errands_settings_set_maximized(ErrandsSettings *self, gboolean maximized);
void errands_settings_set_show_completed(ErrandsSettings *self, gboolean show_completed);
void errands_settings_set_show_cancelled(ErrandsSettings *self, gboolean show_cancelled);
void errands_settings_set_background(ErrandsSettings *self, gboolean background);
void errands_settings_set_startup(ErrandsSettings *self, gboolean startup);
void errands_settings_set_sync_enabled(ErrandsSettings *self, gboolean sync_enabled);
void errands_settings_set_theme(ErrandsSettings *self, gint theme);
void errands_settings_set_sort_by(ErrandsSettings *self, gint sort_by);
void errands_settings_set_sort_order(ErrandsSettings *self, gint sort_order);
void errands_settings_set_sync_interval(ErrandsSettings *self, gint sync_interval);
void errands_settings_set_window_width(ErrandsSettings *self, gint window_width);
void errands_settings_set_window_height(ErrandsSettings *self, gint window_height);
void errands_settings_set_last_list_uid(ErrandsSettings *self, const gchar *last_list_uid);
void errands_settings_set_sync_provider(ErrandsSettings *self, const gchar *sync_provider);
void errands_settings_set_sync_url(ErrandsSettings *self, const gchar *sync_url);
void errands_settings_set_sync_username(ErrandsSettings *self, const gchar *sync_username);
void errands_settings_set_sync_password(ErrandsSettings *self, const gchar *sync_password);
void errands_settings_set_tags(ErrandsSettings *self, GStrv tags);
