#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_WEEK_CHOOSER (errands_task_list_date_dialog_week_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogWeekChooser, errands_task_list_date_dialog_week_chooser, ERRANDS,
                     TASK_LIST_DATE_DIALOG_WEEK_CHOOSER, GtkListBoxRow)

ErrandsTaskListDateDialogWeekChooser *errands_task_list_date_dialog_week_chooser_new();
const char *errands_task_list_date_dialog_week_chooser_get_days(ErrandsTaskListDateDialogWeekChooser *self);
void errands_task_list_date_dialog_week_chooser_set_days(ErrandsTaskListDateDialogWeekChooser *self, const char *rrule);
void errands_task_list_date_dialog_week_chooser_reset(ErrandsTaskListDateDialogWeekChooser *self);
