#pragma once

#include "task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_COLOR_WINDOW (errands_color_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsColorWindow, errands_color_window, ERRANDS,
                     COLOR_WINDOW, AdwDialog)

struct _ErrandsColorWindow {
  AdwDialog parent_instance;
  GtkWidget *color_box;
  ErrandsTask *task;
  bool block_signals;
};

ErrandsColorWindow *errands_color_window_new();
void errands_color_window_show(ErrandsTask *task);
