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
};

ErrandsTaskList *errands_task_list_new();
void errands_task_list_add(TaskData *td);
void errands_task_list_update_title();
GPtrArray *errands_task_list_get_all_tasks();

#define ERRANDS_TYPE_SORT_DIALOG (errands_sort_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSortDialog, errands_sort_dialog, ERRANDS, SORT_DIALOG, AdwDialog)

struct _ErrandsSortDialog {
  AdwDialog parent_instance;
  GtkWidget *show_completed_row;
  GtkWidget *created_row;
  GtkWidget *due_row;
  GtkWidget *priority_row;
  bool block_signals;
  bool sort_changed;
};

void errands_sort_dialog_show();
