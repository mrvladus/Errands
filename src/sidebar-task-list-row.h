#pragma once

#include "data.h"

#include <adwaita.h>

#define ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW (errands_sidebar_task_list_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row, ERRANDS, SIDEBAR_TASK_LIST_ROW,
                     GtkListBoxRow)

struct _ErrandsSidebarTaskListRow {
  GtkListBoxRow parent_instance;
  GtkWidget *color_btn;
  GtkWidget *counter;
  GtkWidget *label;

  GtkDropControllerMotion *drop_motion_ctrl;

  ListData *data;
};

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_new(ListData *data);
void errands_sidebar_task_list_row_update(ErrandsSidebarTaskListRow *self);
ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(ListData *data);
void on_errands_sidebar_task_list_row_activate(GtkListBox *box, ErrandsSidebarTaskListRow *row, gpointer user_data);
