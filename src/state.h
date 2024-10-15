#pragma once

#include "dialogs/delete-list-dialog.h"
#include "dialogs/new-list-dialog.h"
#include "dialogs/rename-list-dialog.h"
#include "no-lists-page.h"
#include "sidebar/sidebar.h"
#include "task-list.h"
#include "task/attachments-window.h"
#include "task/color-window.h"
#include "task/notes-window.h"
#include "task/priority-window.h"
#include "task/tags-window.h"
#include "window.h"

#include <adwaita.h>

// Structure to hold the application state
typedef struct {
  AdwApplication *app;
  ErrandsWindow *main_window;
  GtkWidget *split_view;
  GtkWidget *stack;
  GtkWidget *tags_page;
  ErrandsSidebar *sidebar;

  // Task List Page
  ErrandsTaskList *task_list;
  ErrandsNoListsPage *no_lists_page;

  // Dialog windows
  ErrandsNewListDialog *new_list_dialog;
  ErrandsRenameListDialog *rename_list_dialog;
  ErrandsDeleteListDialog *delete_list_dialog;

  ErrandsNotesWindow *notes_window;
  ErrandsPriorityWindow *priority_window;
  ErrandsTagsWindow *tags_window;
  ErrandsAttachmentsWindow *attachments_window;
  ErrandsColorWindow *color_window;

  // User Data
  GPtrArray *tl_data;
  GPtrArray *t_data;
  GPtrArray *tags_data;
} State;

// Global state object
extern State state;
