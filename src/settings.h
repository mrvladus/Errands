#pragma once

#include <stdbool.h>

typedef enum { SETTING_TYPE_BOOL, SETTING_TYPE_INT, SETTING_TYPE_STRING } ErrandsSettingType;

typedef union {
  int i;
  bool b;
  const char *s;
} ErrandsSetting;

// Initialize settings.json file
void errands_settings_init();
// Get setting
ErrandsSetting errands_settings_get(const char *key, ErrandsSettingType type);
// Set setting
void errands_settings_set(const char *key, ErrandsSettingType type, void *value);
