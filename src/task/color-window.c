#include "color-window.h"
#include "../state.h"
#include "../utils.h"
#include "task.h"

#include <glib/gi18n.h>
#include <stdio.h>

static void on_errands_color_window_close_cb(ErrandsColorWindow *win);
static void on_errands_color_window_color_select_cb(GtkCheckButton *btn, ErrandsColorWindow *win);

G_DEFINE_TYPE(ErrandsColorWindow, errands_color_window, ADW_TYPE_DIALOG)

static void errands_color_window_class_init(ErrandsColorWindowClass *class) {}

static void errands_color_window_init(ErrandsColorWindow *self) {
  LOG("Creating color window");

  g_object_set(self, "title", _("Accent Color"), NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_color_window_close_cb), NULL);

  // Box
  self->color_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

  // --- Buttons --- //

  const char *const colors[8] = {"none",   "blue", "green",  "yellow",
                                 "orange", "red",  "purple", "brown"};

  GtkWidget *group_btn;
  for (int i = 0; i < 8; i++) {
    GtkWidget *btn = gtk_check_button_new();
    g_object_set(btn, "name", colors[i], NULL);
    gtk_widget_add_css_class(btn, "accent-color-btn");
    char class_name[24];
    sprintf(class_name, "%s%s", "accent-color-btn-", colors[i]);
    gtk_widget_add_css_class(btn, class_name);
    g_signal_connect(btn, "toggled", G_CALLBACK(on_errands_color_window_color_select_cb), self);
    if (!strcmp(colors[i], "none"))
      group_btn = btn;
    else
      g_object_set(btn, "group", group_btn, NULL);
    gtk_box_append(GTK_BOX(self->color_box), btn);
  }

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
