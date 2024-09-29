#ifndef ERRANDS_SIDEBAR_H
#define ERRANDS_SIDEBAR_H

#include "data.h"
#include "sidebar-task-list-row.h"
#include <adwaita.h>

void errands_sidebar_build();
GtkWidget *errands_sidebar_get_list_row(const char *uid);
ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(TaskListData *data);

#endif // ERRANDS_SIDEBAR_H
