#pragma once

#include "task.h"

#define ERRANDS_TYPE_DATE_WINDOW (errands_date_window_get_type())

G_DECLARE_FINAL_TYPE(ErrandsDateWindow, errands_date_window, ERRANDS,
                     DATE_WINDOW, AdwDialog)

struct _ErrandsDateWindow {
  AdwDialog parent_instance;
  GtkWidget *start_time_row;
  GtkWidget *start_time_label;
  GtkWidget *start_time_h;
  GtkWidget *start_time_m;
  GtkWidget *start_date_row;
  GtkWidget *start_date_label;
  GtkWidget *start_calendar;
  ErrandsTask *task;
};

ErrandsDateWindow *errands_date_window_new();
void errands_date_window_show(ErrandsTask *task);
