#pragma once

#include "sidebar-task-list-row.h"

#include <adwaita.h>

#define ERRANDS_TYPE_RENAME_LIST_DIALOG (errands_rename_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsRenameListDialog, errands_rename_list_dialog, ERRANDS, RENAME_LIST_DIALOG, AdwAlertDialog)

ErrandsRenameListDialog *errands_rename_list_dialog_new();
void errands_rename_list_dialog_show(ErrandsSidebarTaskListRow *row);
