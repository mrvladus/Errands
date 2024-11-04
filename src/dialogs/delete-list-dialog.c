#include "delete-list-dialog.h"
#include "../data.h"
#include "../sidebar/sidebar-task-list-row.h"
#include "../state.h"
#include "../task/task.h"
#include "../utils.h"
#include "gtk/gtk.h"

#include <glib/gi18n.h>

static void on_response_cb(ErrandsDeleteListDialog *dialog, gchar *response, gpointer data);

G_DEFINE_TYPE(ErrandsDeleteListDialog, errands_delete_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void errands_delete_list_dialog_class_init(ErrandsDeleteListDialogClass *class) {}

static void errands_delete_list_dialog_init(ErrandsDeleteListDialog *self) {
  adw_alert_dialog_set_heading(ADW_ALERT_DIALOG(self), _("Are you sure?"));

  // Dialog
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(self), "cancel", _("Cancel"));
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(self), "confirm", _("Confirm"));
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
  LOG("Show delete dialog for '%s'", row->data->uid);
  char *msg = g_strdup_printf(_("This will completely delete \"%s\" task list"), row->data->name);
  adw_alert_dialog_set_body(ADW_ALERT_DIALOG(state.delete_list_dialog), msg);
  adw_dialog_present(ADW_DIALOG(state.delete_list_dialog), GTK_WIDGET(state.main_window));
  g_free(msg);
}

// --- SIGNAL HANDLERS --- //

static void on_response_cb(ErrandsDeleteListDialog *dialog, gchar *response, gpointer data) {
  if (!strcmp(response, "confirm")) {
    LOG("Delete list dialog: Deleting task list '%s'", dialog->row->data->uid);
    // Delete tasks widgets
    GPtrArray *tasks = get_children(state.task_list->task_list);
    for (int i = 0; i < tasks->len; i++) {
      ErrandsTask *task = tasks->pdata[i];
      if (!strcmp(dialog->row->data->uid, task->data->list_uid))
        gtk_box_remove(GTK_BOX(state.task_list->task_list), GTK_WIDGET(task));
    }
    // Delete data
    errands_data_delete_list(dialog->row->data);
    errands_data_write();

    GtkWidget *prev = gtk_widget_get_prev_sibling(GTK_WIDGET(dialog->row));
    GtkWidget *next = gtk_widget_get_next_sibling(GTK_WIDGET(dialog->row));
    // Delete sidebar row
    gtk_list_box_remove(GTK_LIST_BOX(state.sidebar->task_lists_box), GTK_WIDGET(dialog->row));
    // Switch row
    if (prev) {
      g_signal_emit_by_name(prev, "activate", NULL);
      return;
    }
    if (next) {
      g_signal_emit_by_name(next, "activate", NULL);
      return;
    }
    // TODO: show no lists page

    // TODO: sync
  }
}