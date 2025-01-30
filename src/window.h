#pragma once

#include "adwaita.h"
#include "no-lists-page.h"

#define ERRANDS_TYPE_WINDOW (errands_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsWindow, errands_window, ERRANDS, WINDOW, AdwApplicationWindow)

struct _ErrandsWindow {
  AdwApplicationWindow parent_instance;
  GtkWidget *stack;
  GtkWidget *split_view;
  GtkWidget *toast_overlay;
  ErrandsNoListsPage *no_lists_page;
};

ErrandsWindow *errands_window_new();
void errands_window_build(ErrandsWindow *win);
void errands_window_update(ErrandsWindow *win);
void errands_window_add_toast(ErrandsWindow *win, const char *msg);
