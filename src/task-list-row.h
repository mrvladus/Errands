#pragma once

#include "data.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_ROW (errands_task_list_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListRow, errands_task_list_row, ERRANDS, TASK_LIST_ROW, GtkListBoxRow)

struct _ErrandsTaskListRow {
  GtkListBoxRow parent_instance;

  GtkWidget *color_btn;
  GtkWidget *counter;
  GtkWidget *label;

  GtkDropControllerMotion *drop_motion_ctrl;

  ListData *data;
};

ErrandsTaskListRow *errands_task_list_row_new(ListData *data);
void errands_task_list_row_update(ErrandsTaskListRow *self);
ErrandsTaskListRow *errands_task_list_row_get(ListData *data);
void on_errands_task_list_row_activate(GtkListBox *box, ErrandsTaskListRow *row, gpointer user_data);
