#pragma once

#include "data/data.h"
#include "task-list-color-dialog.h"
#include "task-list-priority-dialog.h"
#include "task-list-sort-dialog.h"
#include "task/task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST (errands_task_list_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskList, errands_task_list, ERRANDS, TASK_LIST, AdwBin)

struct _ErrandsTaskList {
  AdwBin parent_instance;

  GtkWidget *title;
  GtkWidget *task_list;
  GtkWidget *search_btn;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkWidget *entry_rev;

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

  ErrandsTaskListSortDialog *sort_dialog;
  ErrandsTaskListColorDialog *color_dialog;
  ErrandsTaskListPriorityDialog *priority_dialog;

  ErrandsAttachmentsWindow *attachments_window;
  ErrandsNotesWindow *notes_window;
  ErrandsTagsWindow *tags_window;
  ErrandsDateWindow *date_window;

  ListData *data;
};

ErrandsTaskList *errands_task_list_new();
void errands_task_list_load_tasks(ErrandsTaskList *self);
void errands_task_list_add(TaskData *td);
void errands_task_list_update_title();
GPtrArray *errands_task_list_get_all_tasks();
