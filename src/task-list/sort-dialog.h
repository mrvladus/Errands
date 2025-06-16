#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_SORT_DIALOG (errands_sort_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSortDialog, errands_sort_dialog, ERRANDS, SORT_DIALOG, AdwDialog)

struct _ErrandsSortDialog {
  AdwDialog parent_instance;
  GtkWidget *show_completed_row;
  GtkWidget *created_row;
  GtkWidget *due_row;
  GtkWidget *priority_row;
  bool block_signals;
  bool sort_changed;
};

void errands_sort_dialog_show();
