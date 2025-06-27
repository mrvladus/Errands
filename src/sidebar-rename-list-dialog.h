#pragma once

#include "sidebar-task-list-row.h"

#include <adwaita.h>

#define ERRANDS_TYPE_SIDEBAR_RENAME_LIST_DIALOG (errands_sidebar_rename_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarRenameListDialog, errands_sidebar_rename_list_dialog, ERRANDS,
                     SIDEBAR_RENAME_LIST_DIALOG, AdwAlertDialog)

struct _ErrandsSidebarRenameListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
  ErrandsSidebarTaskListRow *current_task_list_row;
};

ErrandsSidebarRenameListDialog *errands_sidebar_rename_list_dialog_new();
void errands_sidebar_rename_list_dialog_show(ErrandsSidebarTaskListRow *row);
