#pragma once

#include "settings.h"
#include "window.h"

// Structure to hold the application state
typedef struct {
  AdwApplication *app;
  ErrandsWindow *main_window;
  ErrandsSettings *settings;
} State;

// Global state object
extern State state;
