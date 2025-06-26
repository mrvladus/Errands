#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_SORT_DIALOG (errands_task_list_sort_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListSortDialog, errands_task_list_sort_dialog, ERRANDS, TASK_LIST_SORT_DIALOG,
                     AdwDialog)

struct _ErrandsTaskListSortDialog {
  AdwDialog parent_instance;
  GtkWidget *completed_toggle_row;
  GtkWidget *creation_date_toggle_btn;
  GtkWidget *due_date_toggle_btn;
  GtkWidget *priority_toggle_btn;
  bool sort_changed;
  bool block_signals;
};

ErrandsTaskListSortDialog *errands_task_list_sort_dialog_new();
void errands_task_list_sort_dialog_show();
