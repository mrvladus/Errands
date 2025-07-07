#pragma once

#include <adwaita.h>
#include <libical/ical.h>

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER (errands_task_list_date_dialog_date_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogDateChooser, errands_task_list_date_dialog_date_chooser, ERRANDS,
                     TASK_LIST_DATE_DIALOG_DATE_CHOOSER, AdwActionRow)

ErrandsTaskListDateDialogDateChooser *errands_task_list_date_dialog_date_chooser_new();
icaltimetype errands_task_list_date_dialog_date_chooser_get_date(ErrandsTaskListDateDialogDateChooser *self);
void errands_task_list_date_dialog_date_chooser_set_date(ErrandsTaskListDateDialogDateChooser *self, icaltimetype date);
void errands_task_list_date_dialog_date_chooser_reset(ErrandsTaskListDateDialogDateChooser *self);
