// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "window.h"
#include "config.h"
#include "settings.h"
#include "state.h"
#include "task-list.h"

#include <adwaita.h>
#include <glib.h>

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsWindow, errands_window, ADW_TYPE_APPLICATION_WINDOW)

static void finalize(GObject *gobject) {
  g_clear_object(&state.settings_dialog);
  g_clear_object(&state.rename_list_dialog);
  g_clear_object(&state.delete_list_dialog);
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_WINDOW);
  G_OBJECT_CLASS(errands_window_parent_class)->finalize(gobject);
}

static void errands_window_class_init(ErrandsWindowClass *class) {
  G_OBJECT_CLASS(class)->finalize = finalize;
  g_type_ensure(ERRANDS_TYPE_SIDEBAR);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST);
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/window.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, toast_overlay);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, split_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, sidebar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, task_list);
}

static void errands_window_init(ErrandsWindow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  // Set theme
  AdwStyleManager *style_manager = adw_style_manager_get_default();
  g_object_set(style_manager, "color-scheme", errands_settings_get_theme(state.settings), NULL);
  g_object_bind_property(state.settings, "theme", style_manager, "color-scheme", G_BINDING_DEFAULT);
  g_object_bind_property(self->split_view, "collapsed", self->task_list->sidebar_btn, "active",
                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL | G_BINDING_INVERT_BOOLEAN);

  g_message("Window: Created");
}

// ---------- PUBLIC FUNCTIONS ---------- //

ErrandsWindow *errands_window_new(GtkApplication *app) {
  ErrandsWindow *self = g_object_new(
      ERRANDS_TYPE_WINDOW, "application", app, "default-width", errands_settings_get_window_width(state.settings),
      "default-height", errands_settings_get_window_height(state.settings), "maximized",
      errands_settings_get_maximized(state.settings), "hide-on-close", errands_settings_get_background(state.settings), NULL);

  g_object_bind_property(state.settings, "maximized", self, "maximized", G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "window-width", self, "default-width", G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "window-height", self, "default-height", G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "background", self, "hide-on-close", G_BINDING_DEFAULT);
  return self;
}

void errands_window_add_toast(const char *msg, int timeout) {
  g_message("Window: Add Toast '%s'", msg);
  AdwToast *toast = adw_toast_new(msg);
  adw_toast_set_timeout(toast, timeout);
  adw_toast_overlay_add_toast(state.main_window->toast_overlay, toast);
}
