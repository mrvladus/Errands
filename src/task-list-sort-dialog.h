#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_SORT_DIALOG (errands_task_list_sort_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListSortDialog, errands_task_list_sort_dialog, ERRANDS, TASK_LIST_SORT_DIALOG,
                     AdwDialog)

ErrandsTaskListSortDialog *errands_task_list_sort_dialog_new();
void errands_task_list_sort_dialog_show();
