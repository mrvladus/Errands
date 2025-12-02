#pragma once

#include "data.h"
#include "gtk/gtk.h"

#include <adwaita.h>

// --- SIDEBAR TASK LIST ROW --- //

#define ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW (errands_sidebar_task_list_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row, ERRANDS, SIDEBAR_TASK_LIST_ROW,
                     GtkListBoxRow)

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

// --- SIDEBAR TODAY ROW --- //

#define ERRANDS_TYPE_SIDEBAR_TODAY_ROW (errands_sidebar_today_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarTodayRow, errands_sidebar_today_row, ERRANDS, SIDEBAR_TODAY_ROW, GtkListBoxRow)

struct _ErrandsSidebarTodayRow {
  GtkListBoxRow parent_instance;
  GtkLabel *counter;
};

ErrandsSidebarTodayRow *errands_sidebar_today_row_new();

// --- SIDEBAR DELETE LIST DIALOG --- //

#define ERRANDS_TYPE_SIDEBAR_DELETE_LIST_DIALOG (errands_sidebar_delete_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarDeleteListDialog, errands_sidebar_delete_list_dialog, ERRANDS,
                     SIDEBAR_DELETE_LIST_DIALOG, AdwAlertDialog)

ErrandsSidebarDeleteListDialog *errands_sidebar_delete_list_dialog_new();
void errands_sidebar_delete_list_dialog_show(ErrandsSidebarTaskListRow *row);

// --- SIDEBAR NEW LIST DIALOG --- //

#define ERRANDS_TYPE_SIDEBAR_NEW_LIST_DIALOG (errands_sidebar_new_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarNewListDialog, errands_sidebar_new_list_dialog, ERRANDS, SIDEBAR_NEW_LIST_DIALOG,
                     AdwAlertDialog)

ErrandsSidebarNewListDialog *errands_sidebar_new_list_dialog_new();
void errands_sidebar_new_list_dialog_show();

// --- SIDEBAR RENAME LIST DIALOG --- //

#define ERRANDS_TYPE_SIDEBAR_RENAME_LIST_DIALOG (errands_sidebar_rename_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarRenameListDialog, errands_sidebar_rename_list_dialog, ERRANDS,
                     SIDEBAR_RENAME_LIST_DIALOG, AdwAlertDialog)

ErrandsSidebarRenameListDialog *errands_sidebar_rename_list_dialog_new();
void errands_sidebar_rename_list_dialog_show(ErrandsSidebarTaskListRow *row);

// --- SIDEBAR --- //

#define ERRANDS_TYPE_SIDEBAR (errands_sidebar_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebar, errands_sidebar, ERRANDS, SIDEBAR, AdwBin)

struct _ErrandsSidebar {
  AdwBin parent_instance;
  GtkWidget *add_btn;
  GtkWidget *filters_box;
  GtkListBoxRow *all_row;
  GtkLabel *all_counter;
  GtkListBoxRow *today_row;
  GtkLabel *today_counter;
  GtkListBoxRow *pinned_row;
  GtkLabel *pinned_counter;
  GtkWidget *task_lists_box;

  ErrandsSidebarTaskListRow *current_task_list_row;

  ErrandsSidebarDeleteListDialog *delete_list_dialog;
  ErrandsSidebarNewListDialog *new_list_dialog;
  ErrandsSidebarRenameListDialog *rename_list_dialog;
};

ErrandsSidebar *errands_sidebar_new();
void errands_sidebar_load_lists(ErrandsSidebar *self);
void errands_sidebar_update_filter_rows(ErrandsSidebar *self);
ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(ErrandsSidebar *sb, ListData *data);
void errands_sidebar_select_last_opened_page();
