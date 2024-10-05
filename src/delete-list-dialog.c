#include "delete-list-dialog.h"
#include "data.h"
#include "sidebar-task-list-row.h"
#include "state.h"
#include "task.h"
#include "utils.h"

static void on_response_cb(ErrandsDeleteListDialog *dialog, gchar *response,
                           gpointer data);

G_DEFINE_TYPE(ErrandsDeleteListDialog, errands_delete_list_dialog,
              ADW_TYPE_ALERT_DIALOG)

static void
errands_delete_list_dialog_class_init(ErrandsDeleteListDialogClass *class) {}

static void errands_delete_list_dialog_init(ErrandsDeleteListDialog *self) {
  adw_alert_dialog_set_heading(ADW_ALERT_DIALOG(self), "Are you sure?");

  // Dialog
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(self), "cancel", "Cancel");
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(self), "confirm", "Confirm");
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(self), "confirm",
                                           ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(self), "cancel");
  adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(self), "cancel");
  g_signal_connect(self, "response", G_CALLBACK(on_response_cb), NULL);
}

ErrandsDeleteListDialog *errands_delete_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_DELETE_LIST_DIALOG, NULL));
}

void errands_delete_list_dialog_show(ErrandsSidebarTaskListRow *row) {
  state.delete_list_dialog->row = row;
  char *msg = g_strdup_printf("This will completely delete \"%s\" task list",
                              row->data->name);
  adw_alert_dialog_set_body(ADW_ALERT_DIALOG(state.delete_list_dialog), msg);
  adw_dialog_present(ADW_DIALOG(state.delete_list_dialog),
                     GTK_WIDGET(state.main_window));
  g_free(msg);
}

// --- SIGNAL HANDLERS --- //

static void on_response_cb(ErrandsDeleteListDialog *dialog, gchar *response,
                           gpointer data) {
  if (!strcmp(response, "confirm")) {
    // Delete tasks widgets
    GPtrArray *tasks = get_children(state.task_list->task_list);
    for (int i = 0; i < tasks->len; i++) {
      ErrandsTask *task = tasks->pdata[i];
      if (!strcmp(dialog->row->data->uid, task->data->list_uid))
        gtk_box_remove(GTK_BOX(state.task_list->task_list), GTK_WIDGET(task));
    }
    // Delete sidebar row
    gtk_list_box_remove(GTK_LIST_BOX(state.sidebar->task_lists_box),
                        GTK_WIDGET(dialog->row));
    // Delete data
    errands_data_delete_list(dialog->row->data);
    errands_data_write();
    // TODO: switch to previous list or "no lists" page
    // TODO: sync
  }
}
