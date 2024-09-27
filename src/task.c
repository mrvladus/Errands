#include "task.h"
#include "adwaita.h"
#include "data.h"
#include "gtk/gtk.h"
#include "gtk/gtkrevealer.h"
#include "state.h"
#include "task-list.h"
#include "task-toolbar.h"
#include "utils.h"

// ---------- SIGNALS ---------- //

static void on_errands_task_complete_btn_toggle(GtkCheckButton *btn,
                                                ErrandsTask *task);
static void on_errands_task_toolbar_btn_toggle(GtkToggleButton *btn,
                                               ErrandsTask *task);
static void on_errands_task_expand_click(GtkGestureClick *self, gint n_press,
                                         gdouble x, gdouble y,
                                         ErrandsTask *task);
static void on_errands_task_sub_task_added(GtkEntry *entry, ErrandsTask *task);

// ---------- TASK ---------- //

G_DEFINE_TYPE(ErrandsTask, errands_task, ADW_TYPE_BIN)

static void errands_task_class_init(ErrandsTaskClass *class) {}

static void errands_task_init(ErrandsTask *self) {
  // Revealer
  self->revealer = gtk_revealer_new();
  adw_bin_set_child(ADW_BIN(self), self->revealer);

  // Task main box
  self->card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_set(self->card, "margin-top", 6, "margin-bottom", 6, "margin-start",
               12, "margin-end", 12, NULL);
  gtk_widget_add_css_class(self->card, "card");
  gtk_revealer_set_child(GTK_REVEALER(self->revealer), self->card);

  // Title list box
  GtkWidget *title_lb = gtk_list_box_new();
  g_object_set(title_lb, "selection-mode", GTK_SELECTION_NONE, NULL);
  gtk_widget_add_css_class(title_lb, "task-title-row");
  gtk_box_append(GTK_BOX(self->card), title_lb);

  // Title row
  self->title_row = adw_action_row_new();
  g_object_set(self->title_row, "hexpand", true, NULL);
  gtk_widget_add_css_class(self->title_row, "task-title-row");
  gtk_widget_set_cursor_from_name(self->title_row, "pointer");
  self->click_ctrl = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(self->click_ctrl), 1);
  gtk_widget_add_controller(self->title_row,
                            GTK_EVENT_CONTROLLER(self->click_ctrl));
  gtk_list_box_append(GTK_LIST_BOX(title_lb), self->title_row);

  // Complete toggle
  self->complete_btn = gtk_check_button_new();
  g_object_set(self->complete_btn, "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(self->complete_btn, "selection-mode");
  adw_action_row_add_prefix(ADW_ACTION_ROW(self->title_row),
                            self->complete_btn);

  // Toolbar toggle
  self->toolbar_btn = gtk_toggle_button_new();
  g_object_set(self->toolbar_btn, "icon-name", "errands-toolbar-symbolic",
               "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(self->toolbar_btn, "flat");
  gtk_widget_add_css_class(self->toolbar_btn, "circular");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->title_row), self->toolbar_btn);

  // Edit row
  GtkWidget *edit_row = adw_entry_row_new();
  g_object_set(edit_row, "title", "Edit", "hexpand", true, NULL);
  g_object_bind_property(self->title_row, "visible", edit_row, "visible",
                         G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_widget_add_css_class(edit_row, "task-title-row");

  // Toolbar revealer
  // ErrandsTaskToolbar *toolbar = errands_task_toolbar_new(td);
  self->toolbar_revealer = gtk_revealer_new();
  g_object_bind_property(self->toolbar_btn, "active", self->toolbar_revealer,
                         "reveal-child", G_BINDING_SYNC_CREATE);
  gtk_box_append(GTK_BOX(self->card), self->toolbar_revealer);

  // Sub-tasks revealer
  self->sub_tasks_revealer = gtk_revealer_new();
  gtk_box_append(GTK_BOX(self->card), self->sub_tasks_revealer);

  // Sub-tasks vbox
  GtkWidget *sub_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_revealer_set_child(GTK_REVEALER(self->sub_tasks_revealer), sub_vbox);

  // Sub-task entry
  self->sub_entry = gtk_entry_new();
  g_object_set(self->sub_entry, "margin-top", 6, "margin-bottom", 6,
               "margin-start", 12, "margin-end", 12, "placeholder-text",
               "Add Sub-Task", NULL);
  gtk_box_append(GTK_BOX(sub_vbox), self->sub_entry);

  // Sub-tasks
  self->sub_tasks = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(self->sub_tasks, "sub-tasks");
  gtk_box_append(GTK_BOX(sub_vbox), self->sub_tasks);
}

ErrandsTask *errands_task_new(TaskData *data) {
  LOG("Creating task '%s'", data->uid);

  ErrandsTask *task = g_object_new(ERRANDS_TYPE_TASK, NULL);
  task->data = data;

  // Setup
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->revealer), true);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(task->title_row),
                                data->text);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(task->complete_btn),
                              data->completed);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(task->toolbar_btn),
                               data->toolbar_shown);
  // Lazy load toolbar
  if (data->toolbar_shown) {
    task->toolbar = errands_task_toolbar_new(data);
    gtk_revealer_set_child(GTK_REVEALER(task->toolbar_revealer),
                           GTK_WIDGET(task->toolbar));
  }
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->sub_tasks_revealer),
                                data->expanded);

  // Load sub-tasks
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!strcmp(td->parent, data->uid))
      gtk_box_append(GTK_BOX(task->sub_tasks),
                     GTK_WIDGET(errands_task_new(td)));
  }
  errands_task_list_sort_by_completion(task->sub_tasks);

  // Connect signals
  g_signal_connect(task->click_ctrl, "released",
                   G_CALLBACK(on_errands_task_expand_click), task);
  g_signal_connect(task->complete_btn, "toggled",
                   G_CALLBACK(on_errands_task_complete_btn_toggle), task);
  g_signal_connect(task->toolbar_btn, "toggled",
                   G_CALLBACK(on_errands_task_toolbar_btn_toggle), task);
  g_signal_connect(task->sub_entry, "activate",
                   G_CALLBACK(on_errands_task_sub_task_added), task);

  return task;
}

static void on_errands_task_complete_btn_toggle(GtkCheckButton *btn,
                                                ErrandsTask *task) {
  LOG("Toggle completion '%s'", task->data->uid);

  task->data->completed = gtk_check_button_get_active(btn);
  task->data->changed_at = get_date_time();
  task->data->synced = false;
  errands_data_write();

  GtkWidget *task_list;
  if (!strcmp(task->data->parent, ""))
    task_list = state.task_list;
  else
    task_list = gtk_widget_get_parent(GTK_WIDGET(task));

  errands_task_list_sort_by_completion(task_list);
}

static void on_errands_task_toolbar_btn_toggle(GtkToggleButton *btn,
                                               ErrandsTask *task) {
  LOG("Toggle toolbar '%s'", task->data->uid);
  // Lazy load toolbar
  if (!task->toolbar) {
    task->toolbar = errands_task_toolbar_new(task->data);
    gtk_revealer_set_child(GTK_REVEALER(task->toolbar_revealer),
                           GTK_WIDGET(task->toolbar));
  }
  task->data->toolbar_shown = gtk_toggle_button_get_active(btn);
  errands_data_write();
}

static void on_errands_task_expand_click(GtkGestureClick *self, gint n_press,
                                         gdouble x, gdouble y,
                                         ErrandsTask *task) {
  task->data->expanded = !task->data->expanded;
  errands_data_write();
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->sub_tasks_revealer),
                                task->data->expanded);
}

static void on_errands_task_sub_task_added(GtkEntry *entry, ErrandsTask *task) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (!strcmp(text, ""))
    return;
  TaskData *new_td = errands_data_add_task((char *)text, task->data->list_uid,
                                           task->data->uid);
  gtk_box_prepend(GTK_BOX(task->sub_tasks),
                  GTK_WIDGET(errands_task_new(new_td)));
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
}
