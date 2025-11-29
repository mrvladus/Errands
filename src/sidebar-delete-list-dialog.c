#include "data.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"
#include "vendor/toolbox.h"
#include <libical/ical.h>

static void on_response_cb(ErrandsSidebarDeleteListDialog *dialog, gchar *response, gpointer data);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsSidebarDeleteListDialog {
  AdwAlertDialog parent_instance;
  ErrandsSidebarTaskListRow *current_task_list_row;
};

G_DEFINE_TYPE(ErrandsSidebarDeleteListDialog, errands_sidebar_delete_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void errands_errands_sidebar_delete_list_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_DELETE_LIST_DIALOG);
  G_OBJECT_CLASS(errands_sidebar_delete_list_dialog_parent_class)->dispose(gobject);
}

static void errands_sidebar_delete_list_dialog_class_init(ErrandsSidebarDeleteListDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_errands_sidebar_delete_list_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-delete-list-dialog.ui");
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_response_cb);
}

static void errands_sidebar_delete_list_dialog_init(ErrandsSidebarDeleteListDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsSidebarDeleteListDialog *errands_sidebar_delete_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_SIDEBAR_DELETE_LIST_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_sidebar_delete_list_dialog_show(ErrandsSidebarTaskListRow *row) {
  if (!state.main_window->sidebar->delete_list_dialog)
    state.main_window->sidebar->delete_list_dialog = errands_sidebar_delete_list_dialog_new();
  state.main_window->sidebar->delete_list_dialog->current_task_list_row = row;
  adw_dialog_present(ADW_DIALOG(state.main_window->sidebar->delete_list_dialog), GTK_WIDGET(state.main_window));
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsSidebarDeleteListDialog *dialog, gchar *response, gpointer data) {
  if (STR_EQUAL(response, "delete")) {
    ErrandsSidebarTaskListRow *row = state.main_window->sidebar->delete_list_dialog->current_task_list_row;
    LOG("Delete List Dialog: Deleting task list %s", errands_data_get_str(row->data->data, DATA_PROP_LIST_UID));
    // Delete data
    errands_data_set_bool(row->data->data, DATA_PROP_DELETED, true);
    errands_data_set_bool(row->data->data, DATA_PROP_SYNCED, false);
    errands_data_write_list(row->data);
    errands_sync_schedule_list(row->data);
    GtkWidget *prev = gtk_widget_get_prev_sibling(GTK_WIDGET(row));
    GtkWidget *next = gtk_widget_get_next_sibling(GTK_WIDGET(row));
    // Delete sidebar row
    gtk_list_box_remove(GTK_LIST_BOX(state.main_window->sidebar->task_lists_box), GTK_WIDGET(row));
    // Switch row
    if (prev) {
      g_signal_emit_by_name(prev, "activate", NULL);
      return;
    }
    if (next) {
      g_signal_emit_by_name(next, "activate", NULL);
      return;
    }
    errands_window_update(state.main_window);
  }
}
