#pragma once

#include <glib.h>

#include <stdbool.h>

typedef enum { SETTING_TYPE_BOOL, SETTING_TYPE_INT, SETTING_TYPE_STRING } ErrandsSettingType;

typedef union {
  int i;
  bool b;
  const char *s;
} ErrandsSetting;

void errands_settings_init();
ErrandsSetting errands_settings_get(const char *key, ErrandsSettingType type);
void errands_settings_set(const char *key, ErrandsSettingType type, void *value);

// Global tags

GStrv errands_settings_get_tags();
void errands_settings_set_tags(GStrv tags);
void errands_settings_add_tag(const char *tag);
void errands_settings_remove_tag(const char *tag);
