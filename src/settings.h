#pragma once

#include <glib.h>

#include <stdbool.h>

typedef enum {
  SETTING_LAST_LIST_UID,
  SETTING_MAXIMIZED,
  SETTING_SHOW_CANCELLED,
  SETTING_SHOW_COMPLETED,
  SETTING_SORT_BY,
  SETTING_SORT_ORDER,
  SETTING_SYNC,
  SETTING_SYNC_PROVIDER,
  SETTING_SYNC_URL,
  SETTING_SYNC_USERNAME,
  SETTING_TAGS,
  SETTING_WINDOW_HEIGHT,
  SETTING_WINDOW_WIDTH,
  SETTING_NOTIFICATIONS,
  SETTING_BACKGROUND,
  SETTING_STARTUP,
  SETTING_THEME,

  SETTINGS_COUNT
} ErrandsSettingsKey;

typedef enum {
  SETTING_TYPE_BOOL,
  SETTING_TYPE_INT,
  SETTING_TYPE_STRING,
} ErrandsSettingType;

typedef enum {
  SORT_TYPE_CREATION_DATE,
  SORT_TYPE_DUE_DATE,
  SORT_TYPE_PRIORITY,
  SORT_TYPE_START_DATE,
} ErrandsSortType;

typedef enum {
  SORT_ORDER_DESC,
  SORT_ORDER_ASC,
} ErrandsSortOrder;

typedef enum {
  SETTING_THEME_SYSTEM,
  SETTING_THEME_LIGHT,
  SETTING_THEME_DARK,
} ErrandsSettingTheme;

typedef union {
  int i;
  bool b;
  const char *s;
} ErrandsSetting;

void errands_settings_init();
ErrandsSetting errands_settings_get(ErrandsSettingsKey key);
void errands_settings_set(ErrandsSettingsKey key, void *value);

// Global tags

GStrv errands_settings_get_tags();
void errands_settings_set_tags(GStrv tags);
void errands_settings_add_tag(const char *tag);
void errands_settings_remove_tag(const char *tag);
