#include "../data/data.h"
#include "../state.h"
#include "../utils.h"
#include "task.h"

#include <glib/gi18n.h>

static void on_errands_color_window_close_cb(ErrandsColorWindow *win);
static void on_errands_color_window_color_select_cb(GtkCheckButton *btn, ErrandsColorWindow *win);

G_DEFINE_TYPE(ErrandsColorWindow, errands_color_window, ADW_TYPE_DIALOG)

static void errands_color_window_class_init(ErrandsColorWindowClass *class) {}

static void errands_color_window_init(ErrandsColorWindow *self) {
  LOG("Color Window: Create");

  g_object_set(self, "title", _("Accent Color"), NULL);
  self->color_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  const char *const colors[8] = {"none", "blue", "green", "yellow", "orange", "red", "purple", "brown"};
  GtkWidget *group_btn;
  for (size_t i = 0; i < 8; i++) {
    GtkWidget *btn = gtk_check_button_new();
    g_object_set(btn, "name", colors[i], NULL);
    gtk_widget_add_css_class(btn, "accent-color-btn");
    char class_name[24];
    sprintf(class_name, "%s%s", "accent-color-btn-", colors[i]);
    gtk_widget_add_css_class(btn, class_name);
    g_signal_connect(btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb), self);
    if (g_str_equal(colors[i], "none")) group_btn = btn;
    else g_object_set(btn, "group", group_btn, NULL);
    gtk_box_append(GTK_BOX(self->color_box), btn);
  }

  // Toolbar view
  GtkWidget *tb = g_object_new(ADW_TYPE_TOOLBAR_VIEW, "content", self->color_box, "margin-start", 12, "margin-end", 12,
                               "margin-bottom", 12, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), adw_header_bar_new());
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsColorWindow *errands_color_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_COLOR_WINDOW, NULL));
}

void errands_color_window_show(ErrandsTask *task) {
  if (!state.main_window->task_list->color_window)
    state.main_window->task_list->color_window = errands_color_window_new();

  state.main_window->task_list->color_window->block_signals = true;
  state.main_window->task_list->color_window->task = task;

  // Select color
  GPtrArray *colors = get_children(state.main_window->task_list->color_window->color_box);
  for (size_t i = 0; i < colors->len; i++) {
    const char *name = gtk_widget_get_name(colors->pdata[i]);
    const char *color = errands_data_get_str(task->data, DATA_PROP_COLOR);
    if (g_str_equal(name, color)) {
      gtk_check_button_set_active(GTK_CHECK_BUTTON(colors->pdata[i]), true);
      break;
    }
  }
  state.main_window->task_list->color_window->block_signals = false;
  // Show dialog
  adw_dialog_present(ADW_DIALOG(state.main_window->task_list->color_window), GTK_WIDGET(state.main_window));
}

// --- SIGNAL HANDLERS --- //

static void on_errands_color_window_close_cb(ErrandsColorWindow *win) {
  LOG("Color Window: Close");
  GPtrArray *colors = get_children(win->color_box);
  for (size_t i = 0; i < colors->len; i++) {
    GtkCheckButton *btn = GTK_CHECK_BUTTON(colors->pdata[i]);
    if (gtk_check_button_get_active(btn)) {
      const char *name = gtk_widget_get_name(GTK_WIDGET(btn));
      if (g_str_equal(name, errands_data_get_str(win->task->data, DATA_PROP_COLOR))) {
        adw_dialog_close(ADW_DIALOG(win));
        return;
      }
      const char *uid = errands_data_get_str(win->task->data, DATA_PROP_UID);
      LOG("Set accent color '%s' to task '%s'", name, uid);
      errands_data_set_str(win->task->data, DATA_PROP_COLOR, name);
      errands_task_update_accent_color(win->task);
      break;
    }
  }
  errands_data_write_list(task_data_get_list(win->task->data));
  adw_dialog_close(ADW_DIALOG(win));
  // TODO: sync
}

static void on_errands_color_window_color_select_cb(GtkCheckButton *btn, ErrandsColorWindow *win) {
  if (!win->block_signals && gtk_check_button_get_active(btn)) { on_errands_color_window_close_cb(win); }
}
