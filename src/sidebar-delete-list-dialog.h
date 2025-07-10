#pragma once

#include "sidebar-task-list-row.h"

#include <adwaita.h>

#define ERRANDS_TYPE_SIDEBAR_DELETE_LIST_DIALOG (errands_sidebar_delete_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarDeleteListDialog, errands_sidebar_delete_list_dialog, ERRANDS,
                     SIDEBAR_DELETE_LIST_DIALOG, AdwAlertDialog)

ErrandsSidebarDeleteListDialog *errands_sidebar_delete_list_dialog_new();
void errands_sidebar_delete_list_dialog_show(ErrandsSidebarTaskListRow *row);
