#pragma once

#include "../data/data.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST (errands_task_list_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskList, errands_task_list, ERRANDS, TASK_LIST, AdwBin)

struct _ErrandsTaskList {
  AdwBin parent_instance;
  GtkWidget *title;
  GtkWidget *search_btn;
  GtkWidget *entry;
  GtkWidget *task_list;
  ListData *data;
};

ErrandsTaskList *errands_task_list_new();
void errands_task_list_add(TaskData *td);
void errands_task_list_update_title();
GPtrArray *errands_task_list_get_all_tasks();
bool errands_task_list_filter_by_text(GtkWidget *task_list, const char *text);
void errands_task_list_filter_by_completion(GtkWidget *task_list, bool show_completed);
void errands_task_list_filter_by_uid(const char *uid);
void errands_task_list_sort_by_completion(GtkWidget *task_list);
void errands_task_list_sort(GtkWidget *task_list);
void errands_task_list_sort_recursive(GtkWidget *task_list);
void errands_task_list_reload();

#define ERRANDS_TYPE_SORT_DIALOG (errands_sort_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSortDialog, errands_sort_dialog, ERRANDS, SORT_DIALOG, AdwDialog)

struct _ErrandsSortDialog {
  AdwDialog parent_instance;
};
void errands_sort_dialog_show();
