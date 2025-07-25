#include "state.h"
#include "widgets.h"

static void on_dialog_close_cb(ErrandsTaskListPriorityDialog *self);
static void on_toggle_cb(ErrandsTaskListPriorityDialog *self, GtkCheckButton *btn);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListPriorityDialog {
  AdwDialog parent_instance;
  GtkWidget *high_row;
  GtkWidget *medium_row;
  GtkWidget *low_row;
  GtkWidget *none_row;
  GtkWidget *custom_row;

  ErrandsTask *current_task;
  bool block_signals;
};

G_DEFINE_TYPE(ErrandsTaskListPriorityDialog, errands_task_list_priority_dialog, ADW_TYPE_DIALOG)

static void errands_task_list_priority_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_PRIORITY_DIALOG);
  G_OBJECT_CLASS(errands_task_list_priority_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_priority_dialog_class_init(ErrandsTaskListPriorityDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_priority_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-priority-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListPriorityDialog, high_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListPriorityDialog, medium_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListPriorityDialog, low_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListPriorityDialog, none_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListPriorityDialog, custom_row);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_toggle_cb);
}

static void errands_task_list_priority_dialog_init(ErrandsTaskListPriorityDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListPriorityDialog *errands_task_list_priority_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_PRIORITY_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_priority_dialog_show(ErrandsTask *task) {
  if (!state.main_window->task_list->priority_dialog)
    state.main_window->task_list->priority_dialog = errands_task_list_priority_dialog_new();
  ErrandsTaskListPriorityDialog *self = state.main_window->task_list->priority_dialog;
  self->current_task = task;
  self->block_signals = true;
  const uint8_t priority = errands_data_get_int(task->data, DATA_PROP_PRIORITY);
  if (priority == 0) adw_action_row_activate(ADW_ACTION_ROW(self->none_row));
  else if (priority == 1) adw_action_row_activate(ADW_ACTION_ROW(self->low_row));
  else if (priority > 1 && priority < 6) adw_action_row_activate(ADW_ACTION_ROW(self->medium_row));
  else if (priority > 5) adw_action_row_activate(ADW_ACTION_ROW(self->high_row));
  adw_spin_row_set_value(ADW_SPIN_ROW(self->custom_row), priority);
  self->block_signals = false;
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
}

// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskListPriorityDialog *self) {
  const uint8_t val = adw_spin_row_get_value(ADW_SPIN_ROW(self->custom_row));
  if (errands_data_get_int(self->current_task->data, DATA_PROP_PRIORITY) != val) {
    errands_data_set_int(self->current_task->data, DATA_PROP_PRIORITY, val);
    errands_data_write_list(task_data_get_list(self->current_task->data));
    // TODO: SYNC
  }
  // Change icon of priority button
  gtk_button_set_icon_name(GTK_BUTTON(self->current_task->priority_btn),
                           val > 0 ? "errands-priority-set-symbolic" : "errands-priority-symbolic");
  // Add css class to priority button
  gtk_widget_set_css_classes(self->current_task->priority_btn, (const char **){NULL});
  gtk_widget_add_css_class(self->current_task->priority_btn, "image-button");
  gtk_widget_add_css_class(self->current_task->priority_btn, "flat");
  if (val == 0) return;
  else if (val > 0 && val < 3) gtk_widget_add_css_class(self->current_task->priority_btn, "priority-low");
  else if (val >= 3 && val < 7) gtk_widget_add_css_class(self->current_task->priority_btn, "priority-medium");
  else if (val >= 7 && val < 10) gtk_widget_add_css_class(self->current_task->priority_btn, "priority-high");
}

static void on_toggle_cb(ErrandsTaskListPriorityDialog *self, GtkCheckButton *btn) {
  if (!gtk_check_button_get_active(btn) || self->block_signals) return;
  const char *name = gtk_widget_get_name(GTK_WIDGET(btn));
  uint8_t val = 0;
  if (g_str_equal(name, "none")) val = 0;
  else if (g_str_equal(name, "low")) val = 1;
  else if (g_str_equal(name, "medium")) val = 5;
  else if (g_str_equal(name, "high")) val = 9;
  adw_spin_row_set_value(ADW_SPIN_ROW(self->custom_row), val);
}
