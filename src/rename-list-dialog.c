#include "rename-list-dialog.h"
#include "data.h"
#include "sidebar-task-list-row.h"
#include "state.h"

static void on_entry_changed_cb(AdwEntryRow *entry,
                                ErrandsRenameListDialog *dialog);
static void on_entry_activate_cb(AdwEntryRow *entry,
                                 ErrandsRenameListDialog *dialog);
static void on_response_cb(ErrandsRenameListDialog *dialog, gchar *response,
                           gpointer data);

G_DEFINE_TYPE(ErrandsRenameListDialog, errands_rename_list_dialog,
              ADW_TYPE_ALERT_DIALOG)

static void
errands_rename_list_dialog_class_init(ErrandsRenameListDialogClass *class) {}

static void errands_rename_list_dialog_init(ErrandsRenameListDialog *self) {
  adw_alert_dialog_set_heading(ADW_ALERT_DIALOG(self), "Rename List");

  // Box
  GtkWidget *box = gtk_list_box_new();
  g_object_set(box, "selection-mode", GTK_SELECTION_NONE, NULL);
  gtk_widget_add_css_class(box, "boxed-list");

  // Entry
  self->entry = adw_entry_row_new();
  g_object_set(self->entry, "title", "New List Name", NULL);
  g_signal_connect(self->entry, "changed", G_CALLBACK(on_entry_changed_cb),
                   self);
  g_signal_connect(self->entry, "entry-activated",
                   G_CALLBACK(on_entry_activate_cb), self);
  gtk_list_box_append(GTK_LIST_BOX(box), self->entry);

  // Dialog
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(self), "cancel", "Cancel");
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(self), "confirm", "Confirm");
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(self), "confirm",
                                           ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(self), "cancel");
  adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(self), "cancel");
  adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(self), box);
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(self), "confirm",
                                        false);
  g_signal_connect(self, "response", G_CALLBACK(on_response_cb), NULL);
}

ErrandsRenameListDialog *errands_rename_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_RENAME_LIST_DIALOG, NULL));
}

void errands_rename_list_dialog_show(ErrandsSidebarTaskListRow *row) {
  state.rename_list_dialog->row = row;
  gtk_editable_set_text(GTK_EDITABLE(state.rename_list_dialog->entry),
                        row->data->name);
  adw_dialog_present(ADW_DIALOG(state.rename_list_dialog), state.main_window);
  gtk_widget_grab_focus(state.rename_list_dialog->entry);
}

// --- SIGNAL HANDLERS --- //

static void on_entry_changed_cb(AdwEntryRow *entry,
                                ErrandsRenameListDialog *dialog) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  const char *list_name = dialog->row->data->name;
  bool enable = strcmp("", text) && strcmp(text, list_name);
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(dialog), "confirm",
                                        enable);
}

static void on_entry_activate_cb(AdwEntryRow *entry,
                                 ErrandsRenameListDialog *dialog) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (!strcmp(text, ""))
    return;
  on_response_cb(dialog, "create", NULL);
  adw_dialog_close(ADW_DIALOG(dialog));
}

static void on_response_cb(ErrandsRenameListDialog *dialog, gchar *response,
                           gpointer data) {
  if (!strcmp(response, "confirm")) {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(dialog->entry));
    free(dialog->row->data->name);
    dialog->row->data->name = strdup(text);
    errands_data_write();
    errands_sidebar_task_list_row_update_title(dialog->row);
    // TODO: update task list title if current uid is the same as this
    // TODO: sync
  }
}
