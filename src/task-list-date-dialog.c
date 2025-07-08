#include "task-list-date-dialog.h"
#include "data/data.h"
#include "state.h"
#include "task-list-date-dialog-date-chooser.h"
#include "task-list-date-dialog-rrule-row.h"
#include "task.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_dialog_close_cb(ErrandsTaskListDateDialog *self);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTaskListDateDialog, errands_task_list_date_dialog, ADW_TYPE_DIALOG)

static void errands_task_list_date_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG);
  G_OBJECT_CLASS(errands_task_list_date_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_class_init(ErrandsTaskListDateDialogClass *class) {
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_TIME_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW);
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, start_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, start_time_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, rrule_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, due_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, due_time_chooser);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
}

static void errands_task_list_date_dialog_init(ErrandsTaskListDateDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialog *errands_task_list_date_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_date_dialog_show(ErrandsTask *task) {
  if (!state.main_window->task_list->date_dialog)
    state.main_window->task_list->date_dialog = errands_task_list_date_dialog_new();
  ErrandsTaskListDateDialog *dialog = state.main_window->task_list->date_dialog;
  dialog->current_task = task;
  TaskData *data = task->data;

  // Reset all rows
  errands_task_list_date_dialog_date_chooser_reset(dialog->start_date_chooser);
  errands_task_list_date_dialog_time_chooser_reset(dialog->start_time_chooser);
  errands_task_list_date_dialog_rrule_row_reset(dialog->rrule_row);
  errands_task_list_date_dialog_date_chooser_reset(dialog->due_date_chooser);
  errands_task_list_date_dialog_time_chooser_reset(dialog->due_time_chooser);

  // Set start dt
  icaltimetype start_dt = errands_data_get_time(data, DATA_PROP_START_TIME);
  errands_task_list_date_dialog_date_chooser_set_date(dialog->start_date_chooser, start_dt);
  errands_task_list_date_dialog_time_chooser_set_time(dialog->start_time_chooser, start_dt);

  // Set due dt
  icaltimetype due_dt = errands_data_get_time(data, DATA_PROP_DUE_TIME);
  errands_task_list_date_dialog_date_chooser_set_date(dialog->due_date_chooser, due_dt);
  errands_task_list_date_dialog_time_chooser_set_time(dialog->due_time_chooser, due_dt);

  // Setup repeat

  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(state.main_window));
}
// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskListDateDialog *self) {
  TaskData *data = self->current_task->data;

  // Set start datetime
  // bool start_date_is_set =
  //     !strcmp(gtk_label_get_label(GTK_LABEL(self->start_date_chooser->label)), _("Not Set")) ? false : true;
  // bool start_time_is_set =
  //     !strcmp(gtk_label_get_label(GTK_LABEL(self->start_time_chooser->label)), _("Not Set")) ? false : true;
  // LOG("%d %d", start_date_is_set, start_time_is_set);
  // g_autoptr(GString) s_dt = g_string_new("");
  // if (start_date_is_set) {
  //   const char *start_date = errands_date_chooser_get_date(self->start_date_chooser);
  //   g_string_append(s_dt, start_date);
  //   if (start_time_is_set) {
  //     const char *start_time = errands_time_chooser_get_time(self->start_time_chooser);
  //     g_string_append_printf(s_dt, "T%sZ", start_time);
  //   }
  //   errands_data_set_str(data, DATA_PROP_START, s_dt->str);
  // } else {
  //   if (start_time_is_set) {
  //     const char *start_time = errands_time_chooser_get_time(self->start_time_chooser);
  //     char *today = get_today_date();
  //     g_string_append_printf(s_dt, "%sT%sZ", today, start_time);
  //     free(today);
  //   }
  // }
  // errands_data_set_str(data, DATA_PROP_START, s_dt->str);

  // Set due datetime
  // bool due_time_is_set =
  //     !strcmp(gtk_label_get_label(GTK_LABEL(self->due_time_chooser->label)), _("Not Set")) ? false : true;
  // bool due_date_is_set =
  //     !strcmp(gtk_label_get_label(GTK_LABEL(self->due_date_chooser->label)), _("Not Set")) ? false : true;
  // g_autoptr(GString) d_dt = g_string_new("");
  // if (due_date_is_set) {
  //   const char *due_date = errands_date_chooser_get_date(self->due_date_chooser);
  //   g_string_append(d_dt, due_date);
  //   if (due_time_is_set) {
  //     const char *due_time = errands_time_chooser_get_time(self->due_time_chooser);
  //     g_string_append_printf(d_dt, "T%sZ", due_time);
  //   }
  //   errands_data_set_str(data, DATA_PROP_DUE, d_dt->str);
  // } else {
  //   if (due_time_is_set) {
  //     const char *due_time = errands_time_chooser_get_time(self->due_time_chooser);
  //     g_string_append_printf(d_dt, "%sT%sZ", get_today_date(), due_time);
  //   }
  // }
  // errands_data_set_str(data, DATA_PROP_DUE, d_dt->str);

  // Check if repeat is enabled

  // errands_data_write_list(task_data_get_list(data));
  // Set date button text
  // errands_task_update_toolbar(self->task);
  // TODO: sync
}
