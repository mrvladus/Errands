#include "window.h"
#include "delete-list-dialog.h"
#include "notes-window.h"
#include "priority-window.h"
#include "rename-list-dialog.h"
#include "sidebar.h"
#include "state.h"
#include "tags-window.h"
#include "task-list.h"
#include "utils.h"

G_DEFINE_TYPE(ErrandsWindow, errands_window, ADW_TYPE_APPLICATION_WINDOW)

static void errands_window_class_init(ErrandsWindowClass *class) {}

static void errands_window_init(ErrandsWindow *self) {
  LOG("Creating main window");

  g_object_set(self, "application", GTK_APPLICATION(state.app), "title",
               "Errands", NULL);
  gtk_window_set_default_size(GTK_WINDOW(self), 800, 600);

  self->stack = adw_view_stack_new();
  self->split_view = adw_navigation_split_view_new();
  adw_application_window_set_content(ADW_APPLICATION_WINDOW(self),
                                     self->split_view);
}

ErrandsWindow *errands_window_new() {
  return g_object_new(ERRANDS_TYPE_WINDOW, NULL);
}

void errands_window_build(ErrandsWindow *win) {
  // Sidebar
  state.sidebar = errands_sidebar_new();
  adw_navigation_split_view_set_sidebar(
      ADW_NAVIGATION_SPLIT_VIEW(win->split_view),
      adw_navigation_page_new(GTK_WIDGET(state.sidebar), "Sidebar"));

  // Content
  adw_navigation_split_view_set_content(
      ADW_NAVIGATION_SPLIT_VIEW(win->split_view),
      adw_navigation_page_new(win->stack, "Content"));

  state.task_list = errands_task_list_new();

  // Dialogs
  state.notes_window = errands_notes_window_new();
  state.priority_window = errands_priority_window_new();
  state.tags_window = errands_tags_window_new();
  state.new_list_dialog = errands_new_list_dialog_new();
  state.rename_list_dialog = errands_rename_list_dialog_new();
  state.delete_list_dialog = errands_delete_list_dialog_new();
  state.no_lists_page = errands_no_lists_page_new();
}
