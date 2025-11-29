#include "data.h"
#include "state.h"
#include "sync.h"
#include "task-list.h"
#include "task.h"

#include "vendor/toolbox.h"
#include <stdbool.h>

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
  const uint8_t priority = errands_data_get_int(task->data->data, DATA_PROP_PRIORITY);
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
  TaskData2 *data = self->current_task->data;
  if (errands_data_get_int(data->data, DATA_PROP_PRIORITY) != val) {
    errands_data_set_int(data->data, DATA_PROP_PRIORITY, val);
    errands_data_write_list(data->list);
    switch (state.main_window->task_list->page) {
    case ERRANDS_TASK_LIST_PAGE_ALL:
    case ERRANDS_TASK_LIST_PAGE_TODAY: errands_data_sort(); break;
    case ERRANDS_TASK_LIST_PAGE_TASK_LIST: errands_list_data_sort(data->list); break;
    case ERRANDS_TASK_LIST_PAGE_TRASH: break;
    }
    errands_task_list_reload(state.main_window->task_list, true);
    errands_sync_schedule_task(data);
  }
  errands_task_update_toolbar(self->current_task);
}

static void on_toggle_cb(ErrandsTaskListPriorityDialog *self, GtkCheckButton *btn) {
  if (!gtk_check_button_get_active(btn) || self->block_signals) return;
  const char *name = gtk_widget_get_name(GTK_WIDGET(btn));
  uint8_t val = 0;
  if (STR_EQUAL(name, "none")) val = 0;
  else if (STR_EQUAL(name, "low")) val = 1;
  else if (STR_EQUAL(name, "medium")) val = 5;
  else if (STR_EQUAL(name, "high")) val = 9;
  adw_spin_row_set_value(ADW_SPIN_ROW(self->custom_row), val);
}
