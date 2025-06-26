#pragma once

#include "data/data.h"
#include "sidebar-new-list-dialog.h"

#include <adwaita.h>

// --- DECLARE WIDGETS --- //

#define ERRANDS_TYPE_SIDEBAR (errands_sidebar_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebar, errands_sidebar, ERRANDS, SIDEBAR, AdwBin)
#define ERRANDS_TYPE_SIDEBAR_ALL_ROW (errands_sidebar_all_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarAllRow, errands_sidebar_all_row, ERRANDS, SIDEBAR_ALL_ROW, GtkListBoxRow)
#define ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW (errands_sidebar_task_list_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row, ERRANDS, SIDEBAR_TASK_LIST_ROW,
                     GtkListBoxRow)

// --- SIDEBAR --- //

struct _ErrandsSidebar {
  AdwBin parent_instance;
  GtkWidget *add_btn;
  GtkWidget *filters_box;
  ErrandsSidebarAllRow *all_row;
  GtkWidget *task_lists_box;
  ErrandsSidebarTaskListRow *current_task_list_row;

  ErrandsSidebarNewListDialog *new_list_dialog;
  GtkWidget *rename_list_dialog;
  GtkWidget *rename_list_dialog_entry;
  GtkWidget *delete_list_dialog;
};

ErrandsSidebar *errands_sidebar_new();
void errands_sidebar_load_lists(ErrandsSidebar *self);
ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(ErrandsSidebar *sb, ListData *data);
void errands_sidebar_select_last_opened_page();
void errands_sidebar_rename_list_dialog_show(ErrandsSidebarTaskListRow *row);
void errands_sidebar_delete_list_dialog_show(ErrandsSidebarTaskListRow *row);

// --- SIDEBAR ALL ROW --- //

struct _ErrandsSidebarAllRow {
  GtkListBoxRow parent_instance;
  GtkWidget *counter;
};

ErrandsSidebarAllRow *errands_sidebar_all_row_new();
void errands_sidebar_all_row_update_counter(ErrandsSidebarAllRow *row);

// --- SIDEBAR TASK LIST ROW --- //

struct _ErrandsSidebarTaskListRow {
  GtkListBoxRow parent_instance;
  GtkWidget *color_btn;
  GtkWidget *counter;
  GtkWidget *label;
  GtkEventController *hover_ctrl;

  ListData *data;
};

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_new(ListData *data);
void errands_sidebar_task_list_row_update_counter(ErrandsSidebarTaskListRow *row);
void errands_sidebar_task_list_row_update_title(ErrandsSidebarTaskListRow *row);
ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(const char *uid);
void on_errands_sidebar_task_list_row_activate(GtkListBox *box, ErrandsSidebarTaskListRow *row, gpointer user_data);
