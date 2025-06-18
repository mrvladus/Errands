#pragma once

#include <glib.h>

#include <stdbool.h>

typedef enum { SETTING_TYPE_BOOL, SETTING_TYPE_INT, SETTING_TYPE_STRING } ErrandsSettingType;

typedef enum { SORT_TYPE_CREATION_DATE, SORT_TYPE_DUE_DATE, SORT_TYPE_PRIORITY } ErrandsSortType;

typedef union {
  int i;
  bool b;
  const char *s;
} ErrandsSetting;

void errands_settings_init();
ErrandsSetting errands_settings_get(const char *key, ErrandsSettingType type);

void errands_settings_set_string(const char *key, const char *value);
void errands_settings_set_int(const char *key, int value);
void errands_settings_set_bool(const char *key, bool value);

// Global tags

GStrv errands_settings_get_tags();
void errands_settings_set_tags(GStrv tags);
void errands_settings_add_tag(const char *tag);
void errands_settings_remove_tag(const char *tag);
