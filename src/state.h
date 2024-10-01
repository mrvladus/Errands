#ifndef ERRANDS_STATE_H
#define ERRANDS_STATE_H

#include "delete-list-dialog.h"
#include "notes-window.h"
#include "priority-window.h"
#include "rename-list-dialog.h"

#include <adwaita.h>

// Structure to hold the application state
typedef struct {
  AdwApplication *app;
  GtkWidget *main_window;
  GtkWidget *split_view;
  GtkWidget *stack;
  GtkWidget *sidebar;
  GtkWidget *tags_page;

  // Task List Page
  GtkWidget *task_list; // Task List Box
  GtkWidget *task_list_entry;
  char *current_uid; // Current list uid

  // Sidebar list boxes
  GtkWidget *filters_list_box;
  GtkWidget *task_lists_list_box;

  // Sidebar filter rows
  GtkWidget *all_row;
  GtkWidget *today_row;
  GtkWidget *tags_row;
  GtkWidget *trash_row;

  ErrandsRenameListDialog *rename_list_dialog;
  ErrandsDeleteListDialog *delete_list_dialog;

  // Dialog windows

  // Notes window
  ErrandsNotesWindow *notes_window;

  AdwDialog *datetime_window;
  AdwDialog *tags_window;

  // Priority
  ErrandsPriorityWindow *priority_window;

  // User Data
  GPtrArray *tl_data;
  GPtrArray *t_data;
  GPtrArray *tags_data;
} State;

// Global state object
extern State state;

#endif // ERRANDS_STATE_H
