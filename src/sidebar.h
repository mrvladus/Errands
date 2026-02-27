#pragma once

#include "data.h"
#include "new-list-dialog.h"
#include "sidebar-task-list-row.h"

#include <adwaita.h>

#define ERRANDS_TYPE_SIDEBAR (errands_sidebar_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebar, errands_sidebar, ERRANDS, SIDEBAR, AdwBin)

struct _ErrandsSidebar {
  AdwBin parent_instance;
  GtkWidget *add_btn;
  GtkWidget *sync_indicator;
  GtkWidget *filters_box;
  GtkListBoxRow *all_row;
  GtkLabel *all_counter;
  GtkListBoxRow *today_row;
  GtkLabel *today_counter;
  GtkWidget *task_lists_box;

  ErrandsSidebarTaskListRow *current_task_list_row;
  ErrandsNewListDialog *new_list_dialog;
};

ErrandsSidebar *errands_sidebar_new(void);
void errands_sidebar_load_lists(void);
void errands_sidebar_update_filter_rows(void);
void errands_sidebar_select_last_opened_page(void);
void errands_sidebar_toggle_sync_indicator(bool on);
ErrandsSidebarTaskListRow *errands_sidebar_find_row(ListData *data);
bool errands_sidebar_row_is_selected(ErrandsSidebarTaskListRow *row);
