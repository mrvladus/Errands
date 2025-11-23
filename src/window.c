#include "window.h"
#include "data.h"
#include "settings.h"
#include "state.h"
#include "task-list.h"
#include "vendor/toolbox.h"

static void on_size_changed_cb(ErrandsWindow *win);
static void on_maximize_changed_cb(ErrandsWindow *win);
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
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/io/github/mrvladus/Errands/ui/window.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, toast_overlay);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, split_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, sidebar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, task_list);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, no_lists_page);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_maximize_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_size_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_new_list_btn_clicked_cb);
}

static void errands_window_init(ErrandsWindow *self) {
  LOG("Window: Create");
  gtk_widget_init_template(GTK_WIDGET(self));
  LOG("Window: Created");
}

ErrandsWindow *errands_window_new(GtkApplication *app) {
  return g_object_new(ERRANDS_TYPE_WINDOW, "application", app, "maximized", errands_settings_get(SETTING_MAXIMIZED).b,
                      "default-width", errands_settings_get(SETTING_WINDOW_WIDTH).i, "default-height",
                      errands_settings_get(SETTING_WINDOW_HEIGHT).i, NULL);
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_window_update(ErrandsWindow *win) {
  LOG("Window: Update");
  size_t total = 0;
  for_range(i, 0, errands_data_lists->len) {
    ListData2 *data = errands_data_lists->pdata[i];
    bool deleted = errands_data_get_bool(data->data, DATA_PROP_DELETED);
    if (!deleted) total++;
  }
  g_object_set(state.main_window->no_lists_page, "visible", total == 0, NULL);
}

void errands_window_add_toast(ErrandsWindow *win, const char *msg) {
  LOG("Window: Add Toast '%s'", msg);
  g_autoptr(AdwToast) toast = adw_toast_new(msg);
  adw_toast_overlay_add_toast(win->toast_overlay, toast);
}

// ---------- CALLBACKS ---------- //

static void on_size_changed_cb(ErrandsWindow *win) {
  int w, h;
  gtk_window_get_default_size(GTK_WINDOW(win), &w, &h);
  errands_settings_set("window_width", SETTING_TYPE_INT, &w);
  errands_settings_set("window_height", SETTING_TYPE_INT, &h);
}

static void on_maximize_changed_cb(ErrandsWindow *win) {
  bool is_maximized = gtk_window_is_maximized(GTK_WINDOW(win));
  errands_settings_set("maximized", SETTING_TYPE_BOOL, &is_maximized);
}

static void on_new_list_btn_clicked_cb() { errands_sidebar_new_list_dialog_show(); }
