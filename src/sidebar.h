// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include "glib.h"
#include "task-list-item.h"

#include <adwaita.h>

#define ERRANDS_TYPE_SIDEBAR (errands_sidebar_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebar, errands_sidebar, ERRANDS, SIDEBAR, AdwBin)

struct _ErrandsSidebar {
  AdwBin parent_instance;
  GtkWidget *popover;
  GtkWidget *entry;
  GtkWidget *create_btn;
  GtkLabel *all_counter;
  GtkLabel *today_counter;
  GtkWidget *sidebar;
  GtkRevealer *sync_indicator;
  AdwSidebarSection *task_lists_section;
};

ErrandsSidebar *errands_sidebar_new(void);
void errands_sidebar_update_filter_rows(void);
void errands_sidebar_select_last_opened_page(void);
void errands_sidebar_task_list_update_counter(const char *uid);
void errands_sidebar_delete_list(const char *uid);
void errands_sidebar_toggle_sync_indicator(gboolean on);
ErrandsTaskListItem *errands_sidebar_find_list(const char *uid);
