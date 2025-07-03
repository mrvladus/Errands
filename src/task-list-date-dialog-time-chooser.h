#pragma once

#include <adwaita.h>
#include <libical/ical.h>

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_TIME_CHOOSER (errands_task_list_date_dialog_time_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogTimeChooser, errands_task_list_date_dialog_time_chooser, ERRANDS,
                     DATE_DIALOG_TIME_CHOOSER, GtkBox)

struct _ErrandsTaskListDateDialogTimeChooser {
  AdwDialog parent_instance;

  GtkWidget *hours;
  GtkWidget *minutes;
  GtkWidget *label;
  GtkWidget *reset_btn;
};

ErrandsTaskListDateDialogTimeChooser *errands_task_list_date_dialog_time_chooser_new();
icaltimetype errands_task_list_date_dialog_time_chooser_get_time(ErrandsTaskListDateDialogTimeChooser *self);
void errands_task_list_date_dialog_time_chooser_set_time(ErrandsTaskListDateDialogTimeChooser *self, icaltimetype time);
void errands_task_list_date_dialog_time_chooser_reset(ErrandsTaskListDateDialogTimeChooser *self);
