#pragma once

#include "new-list-dialog.h"
#include "task-list-item.h"

#include <adwaita.h>

#define ERRANDS_TYPE_SIDEBAR (errands_sidebar_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebar, errands_sidebar, ERRANDS, SIDEBAR, AdwBin)

struct _ErrandsSidebar {
  AdwBin parent_instance;
  GtkWidget *add_btn;
  GtkWidget *sync_indicator;
  GtkLabel *all_counter;
  GtkLabel *today_counter;
  GtkWidget *sidebar;
  AdwSidebarSection *task_lists_section;

  GListStore *task_lists_model;
};

ErrandsSidebar *errands_sidebar_new(void);
void errands_sidebar_load_lists(void);
void errands_sidebar_update_filter_rows(void);
void errands_sidebar_select_last_opened_page(void);
void errands_sidebar_toggle_sync_indicator(bool on);
void errands_sidebar_task_list_update_counter(const char *uid);
ErrandsTaskListItem *errands_sidebar_find_list(const char *uid);
