#include "task.h"
#include "data.h"
#include "notes-window.h"
#include "priority-window.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

// ---------- SIGNALS ---------- //

static void on_errands_task_complete_btn_toggle(GtkCheckButton *btn,
                                                TaskData *data);
static void on_errands_task_toolbar_btn_toggle(GtkToggleButton *btn,
                                               TaskData *data);
static void on_errands_task_expand_click(GtkGestureClick *self, gint n_press,
                                         gdouble x, gdouble y, GtkWidget *task);
static void on_errands_task_sub_task_added(GtkEntry *entry, GtkWidget *task);

// ---------- TASK ---------- //

GtkWidget *errands_task_new(TaskData *td) {
  LOG("Creating task '%s'", td->uid);

  // VBox
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_set(vbox, "margin-top", 6, "margin-bottom", 6, "margin-start", 12,
               "margin-end", 12, NULL);
  gtk_widget_add_css_class(vbox, "card");

  // Revealer
  GtkWidget *rev = gtk_revealer_new();
  g_object_set(rev, "child", vbox, "reveal-child", true, NULL);

  // Title row
  GtkWidget *title_row = adw_action_row_new();
  g_object_set(title_row, "title", td->text, "hexpand", true, NULL);
  gtk_widget_add_css_class(title_row, "task-title-row");
  gtk_widget_set_cursor_from_name(title_row, "pointer");
  GtkGesture *click_ctrl = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click_ctrl), 1);
  gtk_widget_add_controller(title_row, GTK_EVENT_CONTROLLER(click_ctrl));

  // Complete toggle
  GtkWidget *complete_btn = gtk_check_button_new();
  g_object_set(complete_btn, "active", td->completed, "valign",
               GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(complete_btn, "selection-mode");
  g_signal_connect(complete_btn, "toggled",
                   G_CALLBACK(on_errands_task_complete_btn_toggle), td);
  adw_action_row_add_prefix(ADW_ACTION_ROW(title_row), complete_btn);

  // Toolbar toggle
  GtkWidget *toolbar_btn = gtk_toggle_button_new();
  g_object_set(toolbar_btn, "icon-name", "errands-toolbar-symbolic", "valign",
               GTK_ALIGN_CENTER, "active", td->toolbar_shown, NULL);
  gtk_widget_add_css_class(toolbar_btn, "flat");
  gtk_widget_add_css_class(toolbar_btn, "circular");
  g_signal_connect(toolbar_btn, "toggled",
                   G_CALLBACK(on_errands_task_toolbar_btn_toggle), td);
  adw_action_row_add_suffix(ADW_ACTION_ROW(title_row), toolbar_btn);

  // Edit row
  GtkWidget *edit_row = adw_entry_row_new();
  g_object_set(edit_row, "title", "Edit", "hexpand", true, NULL);
  g_object_bind_property(title_row, "visible", edit_row, "visible",
                         G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_widget_add_css_class(edit_row, "task-title-row");

  // Title list box
  GtkWidget *title_lb = gtk_list_box_new();
  g_object_set(title_lb, "selection-mode", GTK_SELECTION_NONE, NULL);
  gtk_widget_add_css_class(title_lb, "task-title-row");
  gtk_list_box_append(GTK_LIST_BOX(title_lb), title_row);
  gtk_list_box_append(GTK_LIST_BOX(title_lb), edit_row);
  gtk_box_append(GTK_BOX(vbox), title_lb);

  // Toolbar hbox
  GtkWidget *tb_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  g_object_set(tb_hbox, "margin-start", 12, "margin-end", 12, "margin-bottom",
               6, "hexpand", true, NULL);

  // Date button
  GtkWidget *tb_date_btn_content = adw_button_content_new();
  g_object_set(tb_date_btn_content, "icon-name", "errands-calendar-symbolic",
               "label", "Date", NULL);
  GtkWidget *tb_date_btn = gtk_button_new();
  g_object_set(tb_date_btn, "child", tb_date_btn_content, NULL);
  gtk_widget_add_css_class(tb_date_btn, "flat");
  gtk_widget_add_css_class(tb_date_btn, "caption");
  gtk_box_append(GTK_BOX(tb_hbox), tb_date_btn);

  // Notes button
  GtkWidget *tb_notes_btn =
      gtk_button_new_from_icon_name("errands-notes-symbolic");
  g_object_set(tb_notes_btn, "halign", GTK_ALIGN_END, "hexpand", true, NULL);
  gtk_widget_add_css_class(tb_notes_btn, "flat");
  g_signal_connect_swapped(tb_notes_btn, "clicked",
                           G_CALLBACK(errands_notes_window_show), td);
  gtk_box_append(GTK_BOX(tb_hbox), tb_notes_btn);

  // Priority button
  GtkWidget *tb_priority_btn =
      gtk_button_new_from_icon_name("errands-priority-symbolic");
  gtk_widget_add_css_class(tb_priority_btn, "flat");
  g_signal_connect_swapped(tb_priority_btn, "clicked",
                           G_CALLBACK(errands_priority_window_show), rev);
  gtk_box_append(GTK_BOX(tb_hbox), tb_priority_btn);

  // Tags button
  GtkWidget *tb_tags_btn =
      gtk_button_new_from_icon_name("errands-tags-symbolic");
  gtk_widget_add_css_class(tb_tags_btn, "flat");
  gtk_box_append(GTK_BOX(tb_hbox), tb_tags_btn);

  // Attachments button
  GtkWidget *tb_attachments_btn =
      gtk_button_new_from_icon_name("errands-attachment-symbolic");
  gtk_widget_add_css_class(tb_attachments_btn, "flat");
  gtk_box_append(GTK_BOX(tb_hbox), tb_attachments_btn);

  // Toolbar revealer
  GtkWidget *tb_rev = gtk_revealer_new();
  g_object_set(tb_rev, "child", tb_hbox, "reveal-child", true, NULL);
  g_object_bind_property(toolbar_btn, "active", tb_rev, "reveal-child",
                         G_BINDING_SYNC_CREATE);
  gtk_box_append(GTK_BOX(vbox), tb_rev);

  // Sub-tasks vbox
  GtkWidget *sub_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  // Sub-task entry
  GtkWidget *sub_entry = gtk_entry_new();
  g_object_set(sub_entry, "margin-top", 6, "margin-bottom", 6, "margin-start",
               12, "margin-end", 12, "placeholder-text", "Add Sub-Task", NULL);
  gtk_box_append(GTK_BOX(sub_vbox), sub_entry);

  // Sub-tasks box
  GtkWidget *sub_tasks = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(sub_tasks, "sub-tasks");
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *data = state.t_data->pdata[i];
    if (!strcmp(data->parent, td->uid))
      gtk_box_append(GTK_BOX(sub_tasks), errands_task_new(data));
  }
  gtk_box_append(GTK_BOX(sub_vbox), sub_tasks);
  errands_task_list_sort_by_completion(sub_tasks);

  // Sub-tasks revealer
  GtkWidget *sub_rev = gtk_revealer_new();
  g_object_set(sub_rev, "child", sub_vbox, "reveal-child", td->expanded, NULL);
  gtk_box_append(GTK_BOX(vbox), sub_rev);

  // Connect signals
  g_signal_connect(click_ctrl, "released",
                   G_CALLBACK(on_errands_task_expand_click), rev);
  g_signal_connect(sub_entry, "activate",
                   G_CALLBACK(on_errands_task_sub_task_added), rev);

  // Set data
  g_object_set_data(G_OBJECT(rev), "task_data", td);
  g_object_set_data(G_OBJECT(rev), "priority_btn", tb_priority_btn);
  g_object_set_data(G_OBJECT(rev), "task_list", sub_tasks);
  g_object_set_data(G_OBJECT(rev), "sub_rev", sub_rev);
  g_object_set_data(G_OBJECT(rev), "card", vbox);

  return rev;
}

static void on_errands_task_complete_btn_toggle(GtkCheckButton *btn,
                                                TaskData *data) {
  LOG("Toggle completion '%s'", data->uid);
  data->completed = gtk_check_button_get_active(btn);
  data->changed_at = get_date_time();
  data->synced = false;
  errands_data_write();

  GtkWidget *task_list;
  if (!strcmp(data->parent, ""))
    task_list = state.task_list;
  else
    task_list = gtk_widget_get_parent(
        gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_REVEALER));

  errands_task_list_sort_by_completion(task_list);
}

static void on_errands_task_toolbar_btn_toggle(GtkToggleButton *btn,
                                               TaskData *data) {
  LOG("Toggle toolbar '%s'", data->uid);
  data->toolbar_shown = gtk_toggle_button_get_active(btn);
  errands_data_write();
}

static void on_errands_task_expand_click(GtkGestureClick *self, gint n_press,
                                         gdouble x, gdouble y,
                                         GtkWidget *task) {
  TaskData *td = g_object_get_data(G_OBJECT(task), "task_data");
  td->expanded = !td->expanded;

  GtkRevealer *sub_rev = g_object_get_data(G_OBJECT(task), "sub_rev");
  gtk_revealer_set_reveal_child(sub_rev, td->expanded);

  errands_data_write();
}

static void on_errands_task_sub_task_added(GtkEntry *entry, GtkWidget *task) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (!strcmp(text, ""))
    return;
  TaskData *td = g_object_get_data(G_OBJECT(task), "task_data");
  TaskData *new_td = errands_data_add_task((char *)text, td->list_uid, td->uid);
  GtkBox *box = g_object_get_data(G_OBJECT(task), "task_list");
  gtk_box_prepend(box, errands_task_new(new_td));
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
}
