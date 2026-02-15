#pragma once

#include "data.h"
#include "gtk/gtk.h"
#include "task-menu.h"

#include <adwaita.h>

// --- TASK LIST DATE DIALOG RRULE ROW --- //

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW (errands_task_list_date_dialog_rrule_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogRruleRow, errands_task_list_date_dialog_rrule_row, ERRANDS,
                     TASK_LIST_DATE_DIALOG_RRULE_ROW, AdwExpanderRow)

ErrandsTaskListDateDialogRruleRow *errands_task_list_date_dialog_rrule_row_new();
struct icalrecurrencetype errands_task_list_date_dialog_rrule_row_get_rrule(ErrandsTaskListDateDialogRruleRow *self);
void errands_task_list_date_dialog_rrule_row_set_rrule(ErrandsTaskListDateDialogRruleRow *self,
                                                       const struct icalrecurrencetype rrule);
void errands_task_list_date_dialog_rrule_row_reset(ErrandsTaskListDateDialogRruleRow *self);

// --- TASK LIST SORT DIALOG --- //

#define ERRANDS_TYPE_TASK_LIST_SORT_DIALOG (errands_task_list_sort_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListSortDialog, errands_task_list_sort_dialog, ERRANDS, TASK_LIST_SORT_DIALOG,
                     AdwDialog)

ErrandsTaskListSortDialog *errands_task_list_sort_dialog_new();
void errands_task_list_sort_dialog_show();

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
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkWidget *entry_box;
  GtkWidget *entry;
  GtkWidget *entry_menu_btn;
  GtkWidget *entry_apply_btn;
  GtkWidget *scrl;
  GtkWidget *list_view;

  ErrandsTaskMenu *task_menu;
  ErrandsTaskListSortDialog *sort_dialog;

  GtkEventControllerMotion *motion_ctrl;

  float x, y;
  ListData *data;
  ErrandsTaskListPage page;

  GtkTreeListModel *tree_model;
};

ErrandsTaskList *errands_task_list_new();
void errands_task_list_update_title(ErrandsTaskList *self);
void errands_task_list_show_all_tasks(ErrandsTaskList *self);
void errands_task_list_show_today_tasks(ErrandsTaskList *self);
void errands_task_list_show_task_list(ErrandsTaskList *self, ListData *data);
