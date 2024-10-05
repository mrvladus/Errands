#ifndef ERRANDS_STATE_H
#define ERRANDS_STATE_H

#include "delete-list-dialog.h"
#include "new-list-dialog.h"
#include "no-lists-page.h"
#include "notes-window.h"
#include "priority-window.h"
#include "rename-list-dialog.h"
#include "sidebar.h"
#include "task-list.h"
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

  // Notes window
  ErrandsNotesWindow *notes_window;
  ErrandsPriorityWindow *priority_window;
  AdwDialog *datetime_window;
  AdwDialog *tags_window;

  // User Data
  GPtrArray *tl_data;
  GPtrArray *t_data;
  GPtrArray *tags_data;
} State;

// Global state object
extern State state;

#endif // ERRANDS_STATE_H
