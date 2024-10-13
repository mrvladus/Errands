#pragma once

#include "../sidebar/sidebar-task-list-row.h"

#include <adwaita.h>

#define ERRANDS_TYPE_DELETE_LIST_DIALOG (errands_delete_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsDeleteListDialog, errands_delete_list_dialog,
                     ERRANDS, DELETE_LIST_DIALOG, AdwAlertDialog)

struct _ErrandsDeleteListDialog {
  AdwAlertDialog parent_instance;
  ErrandsSidebarTaskListRow *row;
};

ErrandsDeleteListDialog *errands_delete_list_dialog_new();
void errands_delete_list_dialog_show(ErrandsSidebarTaskListRow *row);
