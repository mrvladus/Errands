#pragma once

#include "widgets.h"

// Structure to hold the application state
typedef struct {
  bool is_flatpak;
  AdwApplication *app;
  ErrandsWindow *main_window;
} State;

// Global state object
extern State state;
