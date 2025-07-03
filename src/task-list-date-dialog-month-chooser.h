#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_MONTH_CHOOSER (errands_task_list_date_dialog_month_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogMonthChooser, errands_task_list_date_dialog_month_chooser, ERRANDS,
                     TASK_LIST_DATE_DIALOG_MONTH_CHOOSER, GtkListBoxRow)

ErrandsTaskListDateDialogMonthChooser *errands_task_list_date_dialog_month_chooser_new();
const char *errands_task_list_date_dialog_month_chooser_get_months(ErrandsTaskListDateDialogMonthChooser *self);
void errands_task_list_date_dialog_month_chooser_set_months(ErrandsTaskListDateDialogMonthChooser *self,
                                                            const char *rrule);
void errands_task_list_date_dialog_month_chooser_reset(ErrandsTaskListDateDialogMonthChooser *self);
