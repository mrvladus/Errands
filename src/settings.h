#pragma once

#include <stdbool.h>

typedef struct {
  char version[8];
  bool show_completed;
  int window_width;
  int window_height;
  bool maximized;
} ErrandsSettings;

// Initialize settings.json file
void errands_settings_init();
// Migrate from GSettings to settings.json
void errands_settings_migrate();
// Save settings with cooldown period of 1s.
void errands_settings_save();
