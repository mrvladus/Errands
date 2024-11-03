#pragma once

#include "../components/date-chooser.h"
#include "../components/month-chooser.h"
#include "../components/time-chooser.h"
#include "../components/week-chooser.h"
#include "task.h"

#define ERRANDS_TYPE_DATE_WINDOW (errands_date_window_get_type())

G_DECLARE_FINAL_TYPE(ErrandsDateWindow, errands_date_window, ERRANDS, DATE_WINDOW, AdwDialog)

struct _ErrandsDateWindow {
  AdwDialog parent_instance;

  GtkWidget *start_time_row;
  ErrandsTimeChooser *start_time_chooser;
  GtkWidget *start_date_row;
  ErrandsDateChooser *start_date_chooser;

  GtkWidget *repeat_row;
  GtkWidget *frequency_row;
  ErrandsWeekChooser *week_chooser;
  ErrandsMonthChooser *month_chooser;
  GtkWidget *count_row;
  GtkWidget *interval_row;
  GtkWidget *until_row;
  ErrandsDateChooser *until_date_chooser;

  GtkWidget *due_time_row;
  ErrandsTimeChooser *due_time_chooser;
  GtkWidget *due_date_row;
  ErrandsDateChooser *due_date_chooser;

  ErrandsTask *task;
};

ErrandsDateWindow *errands_date_window_new();
void errands_date_window_show(ErrandsTask *task);
