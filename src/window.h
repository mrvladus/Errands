#pragma once

#include "adwaita.h"

#define ERRANDS_TYPE_WINDOW (errands_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsWindow, errands_window, ERRANDS, WINDOW,
                     AdwApplicationWindow)

struct _ErrandsWindow {
  AdwApplicationWindow parent_instance;
  GtkWidget *stack;
  GtkWidget *split_view;
};

ErrandsWindow *errands_window_new();
void errands_window_build(ErrandsWindow *win);
