#ifndef ERRANDS_SIDEBAR_H
#define ERRANDS_SIDEBAR_H

#include "data.h"
#include <adwaita.h>

void errands_sidebar_build();
GtkWidget *errands_sidebar_get_list_row(const char *uid);
void errands_sidebar_add_task_list(TaskListData *data);

#endif // ERRANDS_SIDEBAR_H
