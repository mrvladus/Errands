#include "window.h"
#include "config.h"
#include "new-list-dialog.h"
#include "settings.h"
#include "state.h"
#include "task-list.h"

static void on_new_list_btn_clicked_cb();

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsWindow, errands_window, ADW_TYPE_APPLICATION_WINDOW)

static void errands_window_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_WINDOW);
  G_OBJECT_CLASS(errands_window_parent_class)->dispose(gobject);
}

static void errands_window_class_init(ErrandsWindowClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_window_dispose;
  g_type_ensure(ERRANDS_TYPE_SIDEBAR);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST);
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/window.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, toast_overlay);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, split_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, sidebar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, task_list);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_new_list_btn_clicked_cb);
}

static void errands_window_init(ErrandsWindow *self) {
  g_message("Window: Create");
  gtk_widget_init_template(GTK_WIDGET(self));
  // Set theme
  AdwStyleManager *style_manager = adw_style_manager_get_default();
  g_object_set(style_manager, "color-scheme", errands_settings_get_theme(state.settings), NULL);
  g_object_bind_property(state.settings, "theme", style_manager, "color-scheme", G_BINDING_DEFAULT);
  g_message("Window: Created");
}

ErrandsWindow *errands_window_new(GtkApplication *app) {
  ErrandsWindow *win =
      g_object_new(ERRANDS_TYPE_WINDOW, "application", app, "default-width", errands_settings_get_window_width(state.settings),
                   "default-height", errands_settings_get_window_height(state.settings), "maximized",
                   errands_settings_get_maximized(state.settings), NULL);
  g_object_bind_property(state.settings, "maximized", win, "maximized", G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "window-width", win, "default-width", G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "window-height", win, "default-height", G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL);
  return win;
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_window_add_toast(const char *msg, int timeout) {
  g_message("Window: Add Toast '%s'", msg);
  AdwToast *toast = adw_toast_new(msg);
  adw_toast_set_timeout(toast, timeout);
  adw_toast_overlay_add_toast(state.main_window->toast_overlay, toast);
}

// ---------- CALLBACKS ---------- //

static void on_new_list_btn_clicked_cb() { errands_new_list_dialog_show(); }
