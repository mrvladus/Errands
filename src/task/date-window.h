#pragma once

#include "../components/time-chooser.h"
#include "task.h"

#define ERRANDS_TYPE_DATE_WINDOW (errands_date_window_get_type())

G_DECLARE_FINAL_TYPE(ErrandsDateWindow, errands_date_window, ERRANDS,
                     DATE_WINDOW, AdwDialog)

struct _ErrandsDateWindow {
  AdwDialog parent_instance;

  GtkWidget *start_time_row;
  GtkWidget *start_time_label;
  ErrandsTimeChooser *start_time_chooser;

  // GtkWidget *start_date_row;
  // GtkWidget *start_date_label;
  // GtkWidget *start_calendar;
  GtkWidget *start_date_all_day_row;

  GtkWidget *due_time_row;
  GtkWidget *due_time_label;
  ErrandsTimeChooser *due_time_chooser;
  // GtkWidget *due_time_h;
  // GtkWidget *due_time_m;
  // GtkWidget *due_date_row;
  // GtkWidget *due_date_label;
  // GtkWidget *due_calendar;
  ErrandsTask *task;
};

ErrandsDateWindow *errands_date_window_new();
void errands_date_window_show(ErrandsTask *task);
