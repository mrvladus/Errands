#pragma once

#include "task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_COLOR_DIALOG (errands_task_list_color_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListColorDialog, errands_task_list_color_dialog, ERRANDS, TASK_LIST_COLOR_DIALOG,
                     AdwDialog)

ErrandsTaskListColorDialog *errands_task_list_color_dialog_new();
void errands_task_list_color_dialog_show(ErrandsTask *task);
