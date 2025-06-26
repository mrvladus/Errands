#pragma once

#include "data/data.h"
#include "sidebar-all-row.h"
#include "sidebar-delete-list-dialog.h"
#include "sidebar-new-list-dialog.h"

#include <adwaita.h>

#define ERRANDS_TYPE_SIDEBAR (errands_sidebar_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebar, errands_sidebar, ERRANDS, SIDEBAR, AdwBin)

struct _ErrandsSidebar {
  AdwBin parent_instance;
  GtkWidget *add_btn;
  GtkWidget *filters_box;
  ErrandsSidebarAllRow *all_row;
  GtkWidget *task_lists_box;
  ErrandsSidebarTaskListRow *current_task_list_row;

  ErrandsSidebarDeleteListDialog *delete_list_dialog;
  ErrandsSidebarNewListDialog *new_list_dialog;
  GtkWidget *rename_list_dialog;
  GtkWidget *rename_list_dialog_entry;
};

ErrandsSidebar *errands_sidebar_new();
void errands_sidebar_load_lists(ErrandsSidebar *self);
ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(ErrandsSidebar *sb, ListData *data);
void errands_sidebar_select_last_opened_page();
void errands_sidebar_rename_list_dialog_show(ErrandsSidebarTaskListRow *row);
