#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_SIDEBAR_NEW_LIST_DIALOG (errands_sidebar_new_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarNewListDialog, errands_sidebar_new_list_dialog, ERRANDS, SIDEBAR_NEW_LIST_DIALOG,
                     AdwAlertDialog)

struct _ErrandsSidebarNewListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
};

ErrandsSidebarNewListDialog *errands_sidebar_new_list_dialog_new();
void errands_sidebar_new_list_dialog_show();
