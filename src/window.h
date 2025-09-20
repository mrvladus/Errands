#pragma once

#include "sidebar.h"
#include "task-list.h"

#include <adwaita.h>

#define ERRANDS_TYPE_WINDOW (errands_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsWindow, errands_window, ERRANDS, WINDOW, AdwApplicationWindow)

struct _ErrandsWindow {
  AdwApplicationWindow parent_instance;
  GtkWidget *no_lists_page;
  ErrandsTaskList *task_list;
  GtkWidget *toast_overlay;
  ErrandsSidebar *sidebar;
  GtkWidget *split_view;
};

ErrandsWindow *errands_window_new(GtkApplication *app);
void errands_window_update(ErrandsWindow *win);
void errands_window_add_toast(ErrandsWindow *win, const char *msg);
