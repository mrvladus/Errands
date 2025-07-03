#pragma once

#include "task-list-date-dialog-date-chooser.h"
#include "task-list-date-dialog-month-chooser.h"
#include "task-list-date-dialog-time-chooser.h"
#include "task-list-date-dialog-week-chooser.h"
#include "task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG (errands_task_list_date_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialog, errands_task_list_date_dialog, ERRANDS, TASK_LIST_DATE_DIALOG,
                     AdwDialog)

struct _ErrandsTaskListDateDialog {
  AdwDialog parent_instance;

  GtkWidget *start_date_row;
  ErrandsTaskListDateDialogDateChooser *start_date_chooser;
  GtkWidget *start_time_row;
  ErrandsTaskListDateDialogTimeChooser *start_time_chooser;

  GtkWidget *repeat_row;
  ErrandsTaskListDateDialogWeekChooser *week_chooser;
  ErrandsTaskListDateDialogMonthChooser *month_chooser;
  GtkWidget *freq_row;
  GtkWidget *interval_row;
  GtkWidget *until_row;
  ErrandsTaskListDateDialogDateChooser *until_date_chooser;
  GtkWidget *count_row;

  GtkWidget *due_date_row;
  ErrandsTaskListDateDialogDateChooser *due_date_chooser;
  GtkWidget *due_time_row;
  ErrandsTaskListDateDialogTimeChooser *due_time_chooser;

  ErrandsTask *current_task;
};

ErrandsTaskListDateDialog *errands_task_list_date_dialog_new();
void errands_task_list_date_dialog_show(ErrandsTask *task);
