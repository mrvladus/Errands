#pragma once

#include "dialogs.h"
#include "sidebar.h"
#include "task-list.h"
#include "task-toolbar.h"
#include "window.h"

#include <adwaita.h>

// Structure to hold the application state
typedef struct {
  bool is_flatpak;
  AdwApplication *app;
  ErrandsWindow *main_window;
  ErrandsSidebar *sidebar;
  ErrandsTaskList *task_list;

  // Dialog windows
  ErrandsNewListDialog *new_list_dialog;
  ErrandsRenameListDialog *rename_list_dialog;
  ErrandsDeleteListDialog *delete_list_dialog;

  ErrandsAttachmentsWindow *attachments_window;
  ErrandsColorWindow *color_window;
  ErrandsNotesWindow *notes_window;
  ErrandsPriorityWindow *priority_window;
  ErrandsTagsWindow *tags_window;
  ErrandsDateWindow *date_window;
} State;

// Global state object
extern State state;
