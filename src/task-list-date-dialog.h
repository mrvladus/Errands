#pragma once

#include "task-list-date-dialog-date-chooser.h"
#include "task-list-date-dialog-month-chooser.h"
#include "task-list-date-dialog-rrule-row.h"
#include "task-list-date-dialog-time-chooser.h"
#include "task-list-date-dialog-week-chooser.h"
#include "task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG (errands_task_list_date_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialog, errands_task_list_date_dialog, ERRANDS, TASK_LIST_DATE_DIALOG,
                     AdwDialog)

ErrandsTaskListDateDialog *errands_task_list_date_dialog_new();
void errands_task_list_date_dialog_show(ErrandsTask *task);
