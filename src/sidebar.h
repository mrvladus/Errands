#pragma once

#include "data.h"
#include "sidebar-all-row.h"
#include "sidebar-task-list-row.h"

#include <adwaita.h>

#define ERRANDS_TYPE_SIDEBAR (errands_sidebar_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebar, errands_sidebar, ERRANDS, SIDEBAR, AdwBin)

struct _ErrandsSidebar {
  AdwBin parent_instance;
  GtkWidget *filters_box;
  ErrandsSidebarAllRow *all_row;
  GtkWidget *task_lists_box;
};

ErrandsSidebar *errands_sidebar_new();

// ErrandsSidebarTaskListRow *errands_sidebar_get_list_row(const char *uid);
ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(ErrandsSidebar *sb,
                                                         TaskListData *data);
void errands_sidebar_select_last_opened_page();
