#pragma once

#include "task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_PRIORITY_WINDOW (errands_priority_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsPriorityWindow, errands_priority_window, ERRANDS,
                     PRIORITY_WINDOW, AdwDialog)

struct _ErrandsPriorityWindow {
  AdwDialog parent_instance;
  GtkWidget *none_btn;
  GtkWidget *low_btn;
  GtkWidget *medium_btn;
  GtkWidget *high_btn;
  GtkWidget *custom;
  ErrandsTask *task;
};

ErrandsPriorityWindow *errands_priority_window_new();
void errands_priority_window_show(ErrandsTask *task);
