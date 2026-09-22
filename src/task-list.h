#pragma once

#include "gtk/gtk.h"
#include "task-list-item.h"

#include <adwaita.h>

// --- TASK LIST DATE DIALOG RRULE ROW --- //

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW (errands_task_list_date_dialog_rrule_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogRruleRow, errands_task_list_date_dialog_rrule_row, ERRANDS,
                     TASK_LIST_DATE_DIALOG_RRULE_ROW, AdwExpanderRow)

ErrandsTaskListDateDialogRruleRow *errands_task_list_date_dialog_rrule_row_new();
void errands_task_list_date_dialog_rrule_row_get_rrule(ErrandsTaskListDateDialogRruleRow *self,
                                                       struct icalrecurrencetype *rrule);
void errands_task_list_date_dialog_rrule_row_set_rrule(ErrandsTaskListDateDialogRruleRow *self,
                                                       const struct icalrecurrencetype *rrule);
void errands_task_list_date_dialog_rrule_row_reset(ErrandsTaskListDateDialogRruleRow *self);

// --- TASK LIST --- //

typedef enum {
  ERRANDS_TASK_LIST_PAGE_ALL,
  ERRANDS_TASK_LIST_PAGE_TODAY,
  ERRANDS_TASK_LIST_PAGE_TASK_LIST,
} ErrandsTaskListPage;

#define ERRANDS_TYPE_TASK_LIST (errands_task_list_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskList, errands_task_list, ERRANDS, TASK_LIST, AdwBin)

struct _ErrandsTaskList {
  AdwBin parent_instance;

  GtkWidget *title;
  GtkWidget *search_btn;
  GtkWidget *menu_btn;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkWidget *entry_box;
  GtkWidget *entry;
  GtkWidget *entry_apply_btn;
  GtkWidget *scrl;
  GtkWidget *list_view;

  GListStore *toplevel_tasks_models; // Model of task models
  GtkFlattenListModel *all_tasks_model;
  GtkFilterListModel *today_tasks_model;

  GtkTreeListModel *tree_model;
  GtkFilter *tree_filter;
  GtkFilterListModel *tree_filter_model;
  GtkTreeListRowSorter *tree_sorter;

  GtkFilterListModel *current_model;

  ErrandsTaskListItem *item;
  ErrandsTaskListPage page;
};

ErrandsTaskList *errands_task_list_new();
// Update title and placeholder
void errands_task_list_update(ErrandsTaskList *self);
void errands_task_list_show_all_tasks(ErrandsTaskList *self);
void errands_task_list_show_today_tasks(ErrandsTaskList *self);
void errands_task_list_show_task_list(ErrandsTaskList *self, ErrandsTaskListItem *item);
void errands_task_list_sort(ErrandsTaskList *self, GtkSorterChange change);
void errands_task_list_filter_tree(ErrandsTaskList *self, GtkFilterChange change);
