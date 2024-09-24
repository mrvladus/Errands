#ifndef ERRANDS_SIDEBAR_TASK_LIST_ROW_H
#define ERRANDS_SIDEBAR_TASK_LIST_ROW_H

#include "data.h"
#include <gtk/gtk.h>

GtkWidget *errands_sidebar_task_list_row_new(TaskListData *data);
GtkWidget *errands_sidebar_task_list_row_get(const char *uid);
void errands_sidebar_task_list_row_update_counter(GtkWidget *row);
void errands_sidebar_task_list_row_update_title(GtkWidget *row);
void on_errands_sidebar_task_list_row_activate(GtkListBox *box,
                                               GtkListBoxRow *row,
                                               gpointer user_data);

#endif // ERRANDS_SIDEBAR_TASK_LIST_ROW_H
