#include "color-window.h"
#include "../state.h"
#include "../utils.h"
#include "task.h"

#include <glib/gi18n.h>

static void on_errands_color_window_close_cb(ErrandsColorWindow *win);
static void on_errands_color_window_color_select_cb(GtkCheckButton *btn, ErrandsColorWindow *win);

G_DEFINE_TYPE(ErrandsColorWindow, errands_color_window, ADW_TYPE_DIALOG)

static void errands_color_window_class_init(ErrandsColorWindowClass *class) {}

static void errands_color_window_init(ErrandsColorWindow *self) {
  LOG("Creating color window");

  g_object_set(self, "title", _("Accent Color"), NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_color_window_close_cb), NULL);

  // --- Buttons --- //

  // None button
  GtkWidget *none_btn = gtk_check_button_new();
  g_object_set(none_btn, "name", "none", NULL);
  gtk_widget_add_css_class(none_btn, "accent-color-btn");
  gtk_widget_add_css_class(none_btn, "accent-color-btn-none");
  g_signal_connect(none_btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb), self);

  // Blue button
  GtkWidget *blue_btn = gtk_check_button_new();
  g_object_set(blue_btn, "name", "blue", "group", none_btn, NULL);
  gtk_widget_add_css_class(blue_btn, "accent-color-btn");
  gtk_widget_add_css_class(blue_btn, "accent-color-btn-blue");
  g_signal_connect(blue_btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb), self);

  // Green button
  GtkWidget *green_btn = gtk_check_button_new();
  g_object_set(green_btn, "name", "green", "group", none_btn, NULL);
  gtk_widget_add_css_class(green_btn, "accent-color-btn");
  gtk_widget_add_css_class(green_btn, "accent-color-btn-green");
  g_signal_connect(green_btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb), self);

  // Yellow button
  GtkWidget *yellow_btn = gtk_check_button_new();
  g_object_set(yellow_btn, "name", "yellow", "group", none_btn, NULL);
  gtk_widget_add_css_class(yellow_btn, "accent-color-btn");
  gtk_widget_add_css_class(yellow_btn, "accent-color-btn-yellow");
  g_signal_connect(yellow_btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb),
                   self);

  // Orange button
  GtkWidget *orange_btn = gtk_check_button_new();
  g_object_set(orange_btn, "name", "orange", "group", none_btn, NULL);
  gtk_widget_add_css_class(orange_btn, "accent-color-btn");
  gtk_widget_add_css_class(orange_btn, "accent-color-btn-orange");
  g_signal_connect(orange_btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb),
                   self);

  // Red button
  GtkWidget *red_btn = gtk_check_button_new();
  g_object_set(red_btn, "name", "red", "group", none_btn, NULL);
  gtk_widget_add_css_class(red_btn, "accent-color-btn");
  gtk_widget_add_css_class(red_btn, "accent-color-btn-red");
  g_signal_connect(red_btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb), self);

  // Putple button
  GtkWidget *purple_btn = gtk_check_button_new();
  g_object_set(purple_btn, "name", "purple", "group", none_btn, NULL);
  gtk_widget_add_css_class(purple_btn, "accent-color-btn");
  gtk_widget_add_css_class(purple_btn, "accent-color-btn-purple");
  g_signal_connect(purple_btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb),
                   self);

  // Brown button
  GtkWidget *brown_btn = gtk_check_button_new();
  g_object_set(brown_btn, "name", "brown", "group", none_btn, NULL);
  gtk_widget_add_css_class(brown_btn, "accent-color-btn");
  gtk_widget_add_css_class(brown_btn, "accent-color-btn-brown");
  g_signal_connect(brown_btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb), self);

  // Box
  self->color_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append(GTK_BOX(self->color_box), none_btn);
  gtk_box_append(GTK_BOX(self->color_box), blue_btn);
  gtk_box_append(GTK_BOX(self->color_box), green_btn);
  gtk_box_append(GTK_BOX(self->color_box), yellow_btn);
  gtk_box_append(GTK_BOX(self->color_box), orange_btn);
  gtk_box_append(GTK_BOX(self->color_box), red_btn);
  gtk_box_append(GTK_BOX(self->color_box), purple_btn);
  gtk_box_append(GTK_BOX(self->color_box), brown_btn);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "content", self->color_box, "margin-start", 12, "margin-end", 12,
               "margin-bottom", 12, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), adw_header_bar_new());
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsColorWindow *errands_color_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_COLOR_WINDOW, NULL));
}

void errands_color_window_show(ErrandsTask *task) {
  state.color_window->block_signals = true;
  state.color_window->task = task;
  // Select color
  GPtrArray *colors = get_children(state.color_window->color_box);
  for (int i = 0; i < colors->len; i++) {
    const char *name = gtk_widget_get_name(colors->pdata[i]);
    if (!strcmp(name, "none") && !strcmp(task->data->color, "")) {
      gtk_check_button_set_active(GTK_CHECK_BUTTON(colors->pdata[i]), true);
      break;
    }
    if (!strcmp(name, task->data->color)) {
      gtk_check_button_set_active(GTK_CHECK_BUTTON(colors->pdata[i]), true);
      break;
    }
  }
  g_ptr_array_free(colors, true);
  state.color_window->block_signals = false;
  // Show dialog
  adw_dialog_present(ADW_DIALOG(state.color_window), GTK_WIDGET(state.main_window));
}

// --- SIGNAL HANDLERS --- //

static void on_errands_color_window_close_cb(ErrandsColorWindow *win) {
  GPtrArray *colors = get_children(win->color_box);
  for (int i = 0; i < colors->len; i++) {
    GtkCheckButton *btn = GTK_CHECK_BUTTON(colors->pdata[i]);
    if (gtk_check_button_get_active(btn)) {
      const char *name = gtk_widget_get_name(GTK_WIDGET(btn));
      if (!strcmp(name, "none")) {
        LOG("Clean accent color on task '%s'", win->task->data->uid);
        strcpy(win->task->data->color, "");
        errands_task_update_accent_color(win->task);
      } else {
        LOG("Set accent color '%s' to task '%s'", name, win->task->data->uid);
        strcpy(win->task->data->color, name);
        errands_task_update_accent_color(win->task);
      }
      break;
    }
  }
  g_ptr_array_free(colors, true);
  errands_data_write();
  adw_dialog_close(ADW_DIALOG(win));
  // TODO: sync
}

static void on_errands_color_window_color_select_cb(GtkCheckButton *btn, ErrandsColorWindow *win) {
  if (!win->block_signals)
    on_errands_color_window_close_cb(win);
}
