#include "priority-window.h"
#include "adwaita.h"
#include "data.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "state.h"
#include "utils.h"

#include <string.h>

// --- GLOBAL STATE VARIABLES --- //

// Check buttons
static GtkWidget *none_btn, *low_btn, *medium_btn, *high_btn;
// Spin row for custom value
static GtkWidget *custom;
// Data of the current task
static TaskData *td;

static void on_errands_priority_window_close(AdwDialog *win, gpointer data) {
  int val = adw_spin_row_get_value(ADW_SPIN_ROW(custom));
  if (td->priority != val) {
    td->priority = val;
    errands_data_write();
    // TODO: SYNC
  }
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
  adw_spin_row_set_value(ADW_SPIN_ROW(custom), val);
}

void errands_priority_window_build() {
  LOG("Creating priority window");

  // Header Bar
  GtkWidget *hb = adw_header_bar_new();

  // Box
  GtkWidget *box = gtk_list_box_new();
  g_object_set(box, "selection-mode", GTK_SELECTION_NONE, "margin-start", 12,
               "margin-end", 12, "margin-bottom", 12, NULL);
  gtk_widget_add_css_class(box, "boxed-list");

  // High
  GtkWidget *high = adw_action_row_new();
  g_object_set(high, "title", "High", NULL);
  gtk_widget_add_css_class(high, "priority-high");
  adw_action_row_add_prefix(
      ADW_ACTION_ROW(high),
      gtk_image_new_from_icon_name("errands-priority-symbolic"));
  high_btn = gtk_check_button_new();
  g_signal_connect(high_btn, "activate",
                   G_CALLBACK(on_priority_button_activate), "high");
  adw_action_row_add_suffix(ADW_ACTION_ROW(high), high_btn);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(high), high_btn);
  gtk_list_box_append(GTK_LIST_BOX(box), high);

  // Medium
  GtkWidget *medium = adw_action_row_new();
  g_object_set(medium, "title", "Medium", NULL);
  gtk_widget_add_css_class(medium, "priority-medium");
  adw_action_row_add_prefix(
      ADW_ACTION_ROW(medium),
      gtk_image_new_from_icon_name("errands-priority-symbolic"));
  medium_btn = gtk_check_button_new();
  g_object_set(medium_btn, "group", high_btn, NULL);
  g_signal_connect(medium_btn, "activate",
                   G_CALLBACK(on_priority_button_activate), "medium");
  adw_action_row_add_suffix(ADW_ACTION_ROW(medium), medium_btn);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(medium), medium_btn);
  gtk_list_box_append(GTK_LIST_BOX(box), medium);

  // Low
  GtkWidget *low = adw_action_row_new();
  g_object_set(low, "title", "Low", NULL);
  gtk_widget_add_css_class(low, "priority-low");
  adw_action_row_add_prefix(
      ADW_ACTION_ROW(low),
      gtk_image_new_from_icon_name("errands-priority-symbolic"));
  low_btn = gtk_check_button_new();
  g_object_set(low_btn, "group", high_btn, NULL);
  g_signal_connect(low_btn, "activate", G_CALLBACK(on_priority_button_activate),
                   "low");
  adw_action_row_add_suffix(ADW_ACTION_ROW(low), low_btn);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(low), low_btn);
  gtk_list_box_append(GTK_LIST_BOX(box), low);

  // None
  GtkWidget *none = adw_action_row_new();
  g_object_set(none, "title", "None", NULL);
  adw_action_row_add_prefix(
      ADW_ACTION_ROW(none),
      gtk_image_new_from_icon_name("errands-priority-none-symbolic"));
  none_btn = gtk_check_button_new();
  g_object_set(none_btn, "group", high_btn, NULL);
  g_signal_connect(none_btn, "activate",
                   G_CALLBACK(on_priority_button_activate), "none");
  adw_action_row_add_suffix(ADW_ACTION_ROW(none), none_btn);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(none), none_btn);
  gtk_list_box_append(GTK_LIST_BOX(box), none);

  // Custom
  custom = adw_spin_row_new(gtk_adjustment_new(0, 0, 9, 1, 0, 0), 1, 0);
  g_object_set(custom, "title", "Custom", NULL);
  gtk_list_box_append(GTK_LIST_BOX(box), custom);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "content", box, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);

  // Window
  state.priority_window = g_object_ref_sink(adw_dialog_new());
  g_object_set(state.priority_window, "child", tb, "title", "Priority",
               "content-width", 300, NULL);
  g_signal_connect(state.priority_window, "closed",
                   G_CALLBACK(on_errands_priority_window_close), NULL);
}

void errands_priority_window_show(TaskData *data) {
  adw_dialog_present(state.priority_window, state.main_window);
  td = data;

  // Un-toggle buttons
  gtk_check_button_set_active(GTK_CHECK_BUTTON(none_btn), false);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(low_btn), false);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(medium_btn), false);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(high_btn), false);

  // Toggle for priority
  if (td->priority == 0)
    gtk_check_button_set_active(GTK_CHECK_BUTTON(none_btn), true);
  else if (td->priority == 1)
    gtk_check_button_set_active(GTK_CHECK_BUTTON(low_btn), true);
  else if (td->priority == 5)
    gtk_check_button_set_active(GTK_CHECK_BUTTON(medium_btn), true);
  else if (td->priority == 9)
    gtk_check_button_set_active(GTK_CHECK_BUTTON(high_btn), true);
  adw_spin_row_set_value(ADW_SPIN_ROW(custom), td->priority);
}
