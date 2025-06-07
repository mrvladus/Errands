#include "../components.h"
#include "../data/data.h"
#include "../state.h"
#include "adwaita.h"
#include "task.h"

#include <glib/gi18n.h>
#include <stddef.h>

static void on_errands_priority_window_close(ErrandsPriorityWindow *win, gpointer data);
static void on_priority_button_activate(GtkCheckButton *btn, void *data);

G_DEFINE_TYPE(ErrandsPriorityWindow, errands_priority_window, ADW_TYPE_DIALOG)

static void errands_priority_window_class_init(ErrandsPriorityWindowClass *class) {}

static void errands_priority_window_init(ErrandsPriorityWindow *self) {
  g_object_set(self, "title", _("Priority"), "content-width", 300, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_priority_window_close), NULL);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), adw_header_bar_new());
  adw_dialog_set_child(ADW_DIALOG(self), tb);

  // Box
  GtkWidget *box = gtk_list_box_new();
  g_object_set(box, "selection-mode", GTK_SELECTION_NONE, "margin-start", 12, "margin-end", 12, "margin-bottom", 12,
               NULL);
  gtk_widget_add_css_class(box, "boxed-list");
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), box);

  self->high_row =
      errands_check_row_new(_("High"), "errands-priority-symbolic", NULL, on_priority_button_activate, "high");
  gtk_widget_add_css_class(self->high_row, "priority-high");
  gtk_list_box_append(GTK_LIST_BOX(box), self->high_row);

  self->medium_row = errands_check_row_new(_("Medium"), "errands-priority-symbolic", self->high_row,
                                           on_priority_button_activate, "medium");
  gtk_widget_add_css_class(self->medium_row, "priority-medium");
  gtk_list_box_append(GTK_LIST_BOX(box), self->medium_row);

  self->low_row =
      errands_check_row_new(_("Low"), "errands-priority-symbolic", self->high_row, on_priority_button_activate, "low");
  gtk_widget_add_css_class(self->low_row, "priority-low");
  gtk_list_box_append(GTK_LIST_BOX(box), self->low_row);

  self->none_row = errands_check_row_new(_("None"), "errands-priority-symbolic", self->high_row,
                                         on_priority_button_activate, "none");
  gtk_list_box_append(GTK_LIST_BOX(box), self->none_row);

  // Custom
  self->custom = adw_spin_row_new(gtk_adjustment_new(0, 0, 9, 1, 0, 0), 1, 0);
  g_object_set(self->custom, "title", _("Custom"), NULL);
  gtk_list_box_append(GTK_LIST_BOX(box), self->custom);
}

ErrandsPriorityWindow *errands_priority_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_PRIORITY_WINDOW, NULL));
}

void errands_priority_window_show(ErrandsTask *task) {
  if (!state.priority_window) state.priority_window = errands_priority_window_new();
  adw_dialog_present(ADW_DIALOG(state.priority_window), GTK_WIDGET(state.main_window));
  state.priority_window->task = task;
  const uint8_t priority = errands_data_get_int(task->data, DATA_PROP_PRIORITY);
  if (priority == 0) adw_action_row_activate(ADW_ACTION_ROW(state.priority_window->none_row));
  else if (priority == 1) adw_action_row_activate(ADW_ACTION_ROW(state.priority_window->low_row));
  else if (priority == 5) adw_action_row_activate(ADW_ACTION_ROW(state.priority_window->medium_row));
  else if (priority == 9) adw_action_row_activate(ADW_ACTION_ROW(state.priority_window->high_row));
  adw_spin_row_set_value(ADW_SPIN_ROW(state.priority_window->custom), priority);
}

static void on_errands_priority_window_close(ErrandsPriorityWindow *win, gpointer data) {
  const uint8_t val = adw_spin_row_get_value(ADW_SPIN_ROW(win->custom));
  if (errands_data_get_int(win->task->data, DATA_PROP_PRIORITY) != val) {
    errands_data_set_int(win->task->data, DATA_PROP_PRIORITY, val);
    errands_data_write_list(task_data_get_list(win->task->data));
    // TODO: SYNC
  }

  // Change icon of priority button
  gtk_button_set_icon_name(GTK_BUTTON(win->task->toolbar->priority_btn),
                           val > 0 ? "errands-priority-set-symbolic" : "errands-priority-symbolic");

  // Add css class to priority button
  gtk_widget_set_css_classes(win->task->toolbar->priority_btn, (const char **){NULL});
  gtk_widget_add_css_class(win->task->toolbar->priority_btn, "image-button");
  gtk_widget_add_css_class(win->task->toolbar->priority_btn, "flat");
  if (val == 0) return;
  else if (val > 0 && val < 3) gtk_widget_add_css_class(win->task->toolbar->priority_btn, "priority-low");
  else if (val > 3 && val < 6) gtk_widget_add_css_class(win->task->toolbar->priority_btn, "priority-medium");
  else if (val > 6 && val < 10) gtk_widget_add_css_class(win->task->toolbar->priority_btn, "priority-high");
}

static void on_priority_button_activate(GtkCheckButton *btn, void *data) {
  if (!gtk_check_button_get_active(btn)) return;
  uint8_t val;
  if (!strcmp(data, "none")) val = 0;
  else if (!strcmp(data, "low")) val = 1;
  else if (!strcmp(data, "medium")) val = 5;
  else if (!strcmp(data, "high")) val = 9;
  adw_spin_row_set_value(ADW_SPIN_ROW(state.priority_window->custom), val);
}
