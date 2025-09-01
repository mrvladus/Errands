#pragma once

#include "widgets.h"

// Structure to hold the application state
typedef struct {
  AdwApplication *app;
  ErrandsWindow *main_window;
} State;

// Global state object
extern State state;
