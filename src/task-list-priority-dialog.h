#pragma once

#include "task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_PRIORITY_DIALOG (errands_task_list_priority_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListPriorityDialog, errands_task_list_priority_dialog, ERRANDS,
                     TASK_LIST_PRIORITY_DIALOG, AdwDialog)

ErrandsTaskListPriorityDialog *errands_task_list_priority_dialog_new();
void errands_task_list_priority_dialog_show(ErrandsTask *task);
