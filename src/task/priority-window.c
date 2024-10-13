#include "priority-window.h"
#include "../data.h"
#include "../state.h"

static void on_errands_priority_window_close(ErrandsPriorityWindow *win,
                                             gpointer data);
static void on_priority_button_activate(GtkCheckButton *btn, char *str);

G_DEFINE_TYPE(ErrandsPriorityWindow, errands_priority_window, ADW_TYPE_DIALOG)

static void
errands_priority_window_class_init(ErrandsPriorityWindowClass *class) {}

static void errands_priority_window_init(ErrandsPriorityWindow *self) {
  g_object_set(self, "title", "Priority", "content-width", 300, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_priority_window_close),
                   NULL);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), adw_header_bar_new());
  adw_dialog_set_child(ADW_DIALOG(self), tb);

  // Box
  GtkWidget *box = gtk_list_box_new();
  g_object_set(box, "selection-mode", GTK_SELECTION_NONE, "margin-start", 12,
               "margin-end", 12, "margin-bottom", 12, NULL);
  gtk_widget_add_css_class(box, "boxed-list");
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), box);

  // High
  GtkWidget *high = adw_action_row_new();
  g_object_set(high, "title", "High", NULL);
  gtk_widget_add_css_class(high, "priority-high");
  adw_action_row_add_prefix(
      ADW_ACTION_ROW(high),
      gtk_image_new_from_icon_name("errands-priority-symbolic"));
  self->high_btn = gtk_check_button_new();
  g_signal_connect(self->high_btn, "activate",
                   G_CALLBACK(on_priority_button_activate), "high");
  adw_action_row_add_suffix(ADW_ACTION_ROW(high), self->high_btn);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(high), self->high_btn);
  gtk_list_box_append(GTK_LIST_BOX(box), high);

  // Medium
  GtkWidget *medium = adw_action_row_new();
  g_object_set(medium, "title", "Medium", NULL);
  gtk_widget_add_css_class(medium, "priority-medium");
  adw_action_row_add_prefix(
      ADW_ACTION_ROW(medium),
      gtk_image_new_from_icon_name("errands-priority-symbolic"));
  self->medium_btn = gtk_check_button_new();
  g_object_set(self->medium_btn, "group", self->high_btn, NULL);
  g_signal_connect(self->medium_btn, "activate",
                   G_CALLBACK(on_priority_button_activate), "medium");
  adw_action_row_add_suffix(ADW_ACTION_ROW(medium), self->medium_btn);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(medium),
                                        self->medium_btn);
  gtk_list_box_append(GTK_LIST_BOX(box), medium);

  // Low
  GtkWidget *low = adw_action_row_new();
  g_object_set(low, "title", "Low", NULL);
  gtk_widget_add_css_class(low, "priority-low");
  adw_action_row_add_prefix(
      ADW_ACTION_ROW(low),
      gtk_image_new_from_icon_name("errands-priority-symbolic"));
  self->low_btn = gtk_check_button_new();
  g_object_set(self->low_btn, "group", self->high_btn, NULL);
  g_signal_connect(self->low_btn, "activate",
                   G_CALLBACK(on_priority_button_activate), "low");
  adw_action_row_add_suffix(ADW_ACTION_ROW(low), self->low_btn);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(low), self->low_btn);
  gtk_list_box_append(GTK_LIST_BOX(box), low);

  // None
  GtkWidget *none = adw_action_row_new();
  g_object_set(none, "title", "None", NULL);
  adw_action_row_add_prefix(
      ADW_ACTION_ROW(none),
      gtk_image_new_from_icon_name("errands-priority-none-symbolic"));
  self->none_btn = gtk_check_button_new();
  g_object_set(self->none_btn, "group", self->high_btn, NULL);
  g_signal_connect(self->none_btn, "activate",
                   G_CALLBACK(on_priority_button_activate), "none");
  adw_action_row_add_suffix(ADW_ACTION_ROW(none), self->none_btn);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(none), self->none_btn);
  gtk_list_box_append(GTK_LIST_BOX(box), none);

  // Custom
  self->custom = adw_spin_row_new(gtk_adjustment_new(0, 0, 9, 1, 0, 0), 1, 0);
  g_object_set(self->custom, "title", "Custom", NULL);
  gtk_list_box_append(GTK_LIST_BOX(box), self->custom);
}

ErrandsPriorityWindow *errands_priority_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_PRIORITY_WINDOW, NULL));
}

void errands_priority_window_show(ErrandsTask *task) {
  adw_dialog_present(ADW_DIALOG(state.priority_window),
                     GTK_WIDGET(state.main_window));
  state.priority_window->task = task;

  // Un-toggle buttons
  gtk_check_button_set_active(GTK_CHECK_BUTTON(state.priority_window->none_btn),
                              false);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(state.priority_window->low_btn),
                              false);
  gtk_check_button_set_active(
      GTK_CHECK_BUTTON(state.priority_window->medium_btn), false);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(state.priority_window->high_btn),
                              false);

  // Toggle for priority
  if (task->data->priority == 0)
    gtk_check_button_set_active(
        GTK_CHECK_BUTTON(state.priority_window->none_btn), true);
  else if (task->data->priority == 1)
    gtk_check_button_set_active(
        GTK_CHECK_BUTTON(state.priority_window->low_btn), true);
  else if (task->data->priority == 5)
    gtk_check_button_set_active(
        GTK_CHECK_BUTTON(state.priority_window->medium_btn), true);
  else if (task->data->priority == 9)
    gtk_check_button_set_active(
        GTK_CHECK_BUTTON(state.priority_window->high_btn), true);
  adw_spin_row_set_value(ADW_SPIN_ROW(state.priority_window->custom),
                         task->data->priority);
}

static void on_errands_priority_window_close(ErrandsPriorityWindow *win,
                                             gpointer data) {
  int val = adw_spin_row_get_value(ADW_SPIN_ROW(win->custom));
  if (win->task->data->priority != val) {
    win->task->data->priority = val;
    errands_data_write();
    // TODO: SYNC
  }

  // Change icon of priority button
  gtk_button_set_icon_name(GTK_BUTTON(win->task->toolbar->priority_btn),
                           val > 0 ? "errands-priority-set-symbolic"
                                   : "errands-priority-symbolic");

  // Add css class to priority button
  gtk_widget_set_css_classes(win->task->toolbar->priority_btn,
                             (const char **){NULL});
  gtk_widget_add_css_class(win->task->toolbar->priority_btn, "image-button");
  gtk_widget_add_css_class(win->task->toolbar->priority_btn, "flat");
  if (val == 0)
    return;
  else if (val > 0 && val < 3)
    gtk_widget_add_css_class(win->task->toolbar->priority_btn, "priority-low");
  else if (val > 3 && val < 6)
    gtk_widget_add_css_class(win->task->toolbar->priority_btn,
                             "priority-medium");
  else if (val > 6 && val < 10)
    gtk_widget_add_css_class(win->task->toolbar->priority_btn, "priority-high");
}

static void on_priority_button_activate(GtkCheckButton *btn, char *str) {
  if (!gtk_check_button_get_active(btn))
    return;
  int val;
  if (!strcmp(str, "none"))
    val = 0;
  else if (!strcmp(str, "low"))
    val = 1;
  else if (!strcmp(str, "medium"))
    val = 5;
  else if (!strcmp(str, "high"))
    val = 9;
  adw_spin_row_set_value(ADW_SPIN_ROW(state.priority_window->custom), val);
}
