#ifndef ERRANDS_TASK_LIST_H
#define ERRANDS_TASK_LIST_H

#include "data.h"

// Build Task List
void errands_task_list_build();
// Add new Task to Task List
void errands_task_list_add(TaskData *td);
// Load tasks from user data
void errands_task_list_load_tasks();
// Show tasks only for given list uid
void errands_task_list_filter_by_uid(const char *uid);
void errands_task_list_filter_by_text(const char *text);
// Sort tasks by completion in the given task list box
void errands_task_list_sort_by_completion(GtkWidget *task_list);

#endif // ERRANDS_TASK_LIST_H
