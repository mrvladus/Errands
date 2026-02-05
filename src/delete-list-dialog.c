#include "delete-list-dialog.h"
#include "data.h"
#include "state.h"
#include "sync.h"
#include "task-list.h"

static void on_response_cb(ErrandsDeleteListDialog *dialog, gchar *response, gpointer data);

static ErrandsDeleteListDialog *self = NULL;

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsDeleteListDialog {
  AdwAlertDialog parent_instance;
  ErrandsSidebarTaskListRow *current_task_list_row;
};

G_DEFINE_TYPE(ErrandsDeleteListDialog, errands_delete_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void errands_delete_list_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_DELETE_LIST_DIALOG);
  G_OBJECT_CLASS(errands_delete_list_dialog_parent_class)->dispose(gobject);
}

static void errands_delete_list_dialog_class_init(ErrandsDeleteListDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_delete_list_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/delete-list-dialog.ui");
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_response_cb);
}

static void errands_delete_list_dialog_init(ErrandsDeleteListDialog *dialog) {
  gtk_widget_init_template(GTK_WIDGET(dialog));
}

ErrandsDeleteListDialog *errands_delete_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_DELETE_LIST_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_delete_list_dialog_show(ErrandsSidebarTaskListRow *row) {
  if (!self) self = errands_delete_list_dialog_new();
  self->current_task_list_row = row;
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsDeleteListDialog *dialog, gchar *response, gpointer data) {
  if (STR_EQUAL(response, "delete")) {
    ErrandsSidebarTaskListRow *row = dialog->current_task_list_row;
    LOG("Delete List Dialog: Deleting task list %s", row->data->uid);
    // Delete data
    errands_data_set_deleted(row->data->ical, true);
    errands_list_data_save(row->data);
    errands_sync_delete_list(row->data);
    errands_task_list_reload(state.main_window->task_list, false);
    errands_sidebar_update_filter_rows();
    GtkWidget *prev = gtk_widget_get_prev_sibling(GTK_WIDGET(row));
    GtkWidget *next = gtk_widget_get_next_sibling(GTK_WIDGET(row));
    // Delete sidebar row
    gtk_list_box_remove(GTK_LIST_BOX(state.main_window->sidebar->task_lists_box), GTK_WIDGET(row));
    // Switch row
    if (prev) {
      g_signal_emit_by_name(prev, "activate", NULL);
      errands_task_list_reload(state.main_window->task_list, false);
      return;
    }
    if (next) {
      g_signal_emit_by_name(next, "activate", NULL);
      errands_task_list_reload(state.main_window->task_list, false);
      return;
    }
    g_signal_emit_by_name(state.main_window->sidebar->all_row, "activate", NULL);
  }
}
