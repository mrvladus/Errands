#include "window.h"
#include "data/data.h"
#include "no-lists-page.h"
#include "settings.h"
#include "sidebar/sidebar.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

#include <glib/gi18n.h>

static void on_size_changed(ErrandsWindow *win);
static void on_state_changed(ErrandsWindow *win);

G_DEFINE_TYPE(ErrandsWindow, errands_window, ADW_TYPE_APPLICATION_WINDOW)

static void errands_window_class_init(ErrandsWindowClass *class) {}

static void errands_window_init(ErrandsWindow *self) {
  LOG("Main Window: Create");
  g_object_set(self, "application", GTK_APPLICATION(state.app), "title", _("Errands"), NULL);
  self->stack = adw_view_stack_new();
  self->split_view = adw_navigation_split_view_new();
  self->no_lists_page = errands_no_lists_page_new();
  GtkWidget *overlay = gtk_overlay_new();
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), GTK_WIDGET(self->no_lists_page));
  gtk_overlay_set_child(GTK_OVERLAY(overlay), self->split_view);
  self->toast_overlay = adw_toast_overlay_new();
  adw_toast_overlay_set_child(ADW_TOAST_OVERLAY(self->toast_overlay), overlay);
  adw_application_window_set_content(ADW_APPLICATION_WINDOW(self), self->toast_overlay);
}

ErrandsWindow *errands_window_new() {
  ErrandsWindow *self = g_object_new(ERRANDS_TYPE_WINDOW, NULL);
  if (errands_settings_get("maximized", SETTING_TYPE_BOOL).b) gtk_window_maximize(GTK_WINDOW(self));
  gtk_window_set_default_size(GTK_WINDOW(self), errands_settings_get("window_width", SETTING_TYPE_INT).i,
                              errands_settings_get("window_height", SETTING_TYPE_INT).i);
  g_signal_connect_swapped(self, "notify::default-width", G_CALLBACK(on_size_changed), self);
  g_signal_connect_swapped(self, "notify::default-height", G_CALLBACK(on_size_changed), self);
  g_signal_connect_swapped(self, "notify::maximized", G_CALLBACK(on_state_changed), self);
  return self;
}

void errands_window_build(ErrandsWindow *win) {
  // Sidebar
  state.sidebar = errands_sidebar_new();
  adw_navigation_split_view_set_sidebar(ADW_NAVIGATION_SPLIT_VIEW(win->split_view),
                                        adw_navigation_page_new(GTK_WIDGET(state.sidebar), "Sidebar"));

  // Content
  adw_navigation_split_view_set_content(ADW_NAVIGATION_SPLIT_VIEW(win->split_view),
                                        adw_navigation_page_new(win->stack, "Content"));

  state.task_list = errands_task_list_new();
}

void errands_window_update(ErrandsWindow *win) {
  LOG("Main Window: Update");
  int count = 0;
  GPtrArray *lists = g_hash_table_get_values_as_ptr_array(ldata);
  for (int i = 0; i < lists->len; i++) {
    ListData *ld = lists->pdata[i];
    if (!errands_data_get_bool(ld, DATA_PROP_DELETED)) count++;
  }
  g_object_set(state.main_window->no_lists_page, "visible", count == 0, NULL);
}

void errands_window_add_toast(ErrandsWindow *win, const char *msg) {
  adw_toast_overlay_add_toast(ADW_TOAST_OVERLAY(win->toast_overlay), adw_toast_new(msg));
}

// --- SIGNAL HANDLERS --- //

static void on_size_changed(ErrandsWindow *win) {
  int w, h;
  gtk_window_get_default_size(GTK_WINDOW(win), &w, &h);
  errands_settings_set("window_width", SETTING_TYPE_INT, &w);
  errands_settings_set("window_height", SETTING_TYPE_INT, &h);
}

static void on_state_changed(ErrandsWindow *win) {
  bool m = gtk_window_is_maximized(GTK_WINDOW(win));
  errands_settings_set("maximized", SETTING_TYPE_BOOL, &m);
}
