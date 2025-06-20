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

  GListStore *tasks_model;
  GtkCustomFilter *toplevel_tasks_filter;
  GtkFilterListModel *toplevel_tasks_filter_model;
  GtkCustomSorter *tasks_sorter;
  GtkSortListModel *sort_model;
  GtkCustomSorter *completion_sorter;
  GtkSortListModel *completion_sort_model;
  GtkCustomFilter *search_filter;
  GtkFilterListModel *search_filter_model;
  const char *search_query;
  GtkNoSelection *selection_model;
};

ErrandsTaskList *errands_task_list_new();
void errands_task_list_load_tasks(ErrandsTaskList *self);
void errands_task_list_add(TaskData *td);
void errands_task_list_update_title();
GPtrArray *errands_task_list_get_all_tasks();
