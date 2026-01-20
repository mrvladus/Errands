#include "data.h"
#include "glib.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"

static void on_response_cb(ErrandsSidebarRenameListDialog *self, gchar *response, gpointer data);
static void on_entry_changed_cb(ErrandsSidebarRenameListDialog *self, AdwEntryRow *entry);
static void on_entry_activated_cb(ErrandsSidebarRenameListDialog *self, AdwEntryRow *entry);

static ErrandsSidebarRenameListDialog *self = NULL;

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsSidebarRenameListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
  ErrandsSidebarTaskListRow *current_task_list_row;
};

G_DEFINE_TYPE(ErrandsSidebarRenameListDialog, errands_sidebar_rename_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void errands_errands_sidebar_rename_list_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_RENAME_LIST_DIALOG);
  G_OBJECT_CLASS(errands_sidebar_rename_list_dialog_parent_class)->dispose(gobject);
}

static void errands_sidebar_rename_list_dialog_class_init(ErrandsSidebarRenameListDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_errands_sidebar_rename_list_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-rename-list-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarRenameListDialog, entry);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_response_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_activated_cb);
}

static void errands_sidebar_rename_list_dialog_init(ErrandsSidebarRenameListDialog *dialog) {
  gtk_widget_init_template(GTK_WIDGET(dialog));
}

ErrandsSidebarRenameListDialog *errands_sidebar_rename_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_SIDEBAR_RENAME_LIST_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_sidebar_rename_list_dialog_show(ErrandsSidebarTaskListRow *row) {
  if (!self) self = errands_sidebar_rename_list_dialog_new();
  self->current_task_list_row = row;
  LOG("Sidebar Rename List Dialog: Show");
  gtk_editable_set_text(GTK_EDITABLE(self->entry), errands_data_get_list_name(row->data->ical));
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
  gtk_widget_grab_focus(self->entry);
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsSidebarRenameListDialog *self, gchar *response, gpointer data) {
  if (STR_EQUAL(response, "rename")) {
    ListData *list_data = self->current_task_list_row->data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(self->entry));
    LOG("Sidebar Rename List Dialog: Rename to '%s'", text);
    errands_data_set_list_name(list_data->ical, text);
    errands_data_set_synced(list_data->ical, false);
    errands_list_data_save(list_data);
    errands_sidebar_task_list_row_update(self->current_task_list_row);
    errands_task_list_update_title(state.main_window->task_list);
    // errands_sync_schedule_list(list_data);
  }
}

static void on_entry_changed_cb(ErrandsSidebarRenameListDialog *self, AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  const char *list_name = errands_data_get_list_name(self->current_task_list_row->data->ical);
  const bool enable = !STR_EQUAL("", text) && !STR_EQUAL(text, list_name);
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(self), "rename", enable);
}

static void on_entry_activated_cb(ErrandsSidebarRenameListDialog *self, AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (STR_EQUAL(text, "")) return;
  on_response_cb(self, "rename", NULL);
  adw_dialog_close(ADW_DIALOG(self));
}
