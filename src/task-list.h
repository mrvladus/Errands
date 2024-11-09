#pragma once

#include "data.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST (errands_task_list_get_type())

G_DECLARE_FINAL_TYPE(ErrandsTaskList, errands_task_list, ERRANDS, TASK_LIST, AdwBin)

struct _ErrandsTaskList {
  AdwBin parent_instance;
  GtkWidget *title;
  GtkWidget *search_btn;
  GtkWidget *entry;
  GtkWidget *task_list;
  TaskListData *data;
};

ErrandsTaskList *errands_task_list_new();

// Add new Task to Task List
void errands_task_list_add(TaskData *td);
void errands_task_list_update_title();
void errands_task_list_filter_by_completion(GtkWidget *task_list, bool show_completed);
void errands_task_list_filter_by_uid(const char *uid);
void errands_task_list_filter_by_text(const char *text);
// Sort tasks by completion in the given task list box
void errands_task_list_sort_by_completion(GtkWidget *task_list);
