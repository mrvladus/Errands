#pragma once

#include <adwaita.h>

#include "../sidebar/sidebar.h"

// --- DECLARE WIDGETS --- //

#define ERRANDS_TYPE_NEW_LIST_DIALOG (errands_new_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsNewListDialog, errands_new_list_dialog, ERRANDS, NEW_LIST_DIALOG, AdwAlertDialog)
#define ERRANDS_TYPE_RENAME_LIST_DIALOG (errands_rename_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsRenameListDialog, rename_list_dialog, ERRANDS, RENAME_LIST_DIALOG, AdwAlertDialog)
#define ERRANDS_TYPE_DELETE_LIST_DIALOG (errands_delete_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsDeleteListDialog, errands_delete_list_dialog, ERRANDS, DELETE_LIST_DIALOG, AdwAlertDialog)

// --- NEW LIST DIALOG --- //

struct _ErrandsNewListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
};
ErrandsNewListDialog *errands_new_list_dialog_new();
void errands_new_list_dialog_show();

// --- RENAME LIST DIALOG --- //

struct _ErrandsRenameListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
  ErrandsSidebarTaskListRow *row;
};
ErrandsRenameListDialog *errands_rename_list_dialog_new();
void errands_rename_list_dialog_show(ErrandsSidebarTaskListRow *row);

// --- DELETE LIST DIALOG --- //

struct _ErrandsDeleteListDialog {
  AdwAlertDialog parent_instance;
  ErrandsSidebarTaskListRow *row;
};
ErrandsDeleteListDialog *errands_delete_list_dialog_new();
void errands_delete_list_dialog_show(ErrandsSidebarTaskListRow *row);
