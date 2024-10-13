#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_NEW_LIST_DIALOG (errands_new_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsNewListDialog, errands_new_list_dialog, ERRANDS,
                     NEW_LIST_DIALOG, AdwAlertDialog)

struct _ErrandsNewListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
};

ErrandsNewListDialog *errands_new_list_dialog_new();

void errands_new_list_dialog_show();
