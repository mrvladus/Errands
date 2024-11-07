#pragma once

#include <stdbool.h>

// Initialize settings.json file
void errands_settings_init();

bool errands_settings_get_bool(const char *key);
int errands_settings_get_int(const char *key);
char *errands_settings_get_str(const char *key);

// Set setting. "type" can be "int", "bool", "string"
void errands_settings_set(const char *key, const char *type, void *value);
