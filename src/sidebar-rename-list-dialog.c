#include "sidebar-rename-list-dialog.h"
#include "sidebar-task-list-row.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

static void on_response_cb(ErrandsSidebarRenameListDialog *dialog, gchar *response, gpointer data);
static void on_entry_changed_cb(AdwEntryRow *entry);
static void on_entry_activated_cb(AdwEntryRow *entry);

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

static void errands_sidebar_rename_list_dialog_init(ErrandsSidebarRenameListDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsSidebarRenameListDialog *errands_sidebar_rename_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_SIDEBAR_RENAME_LIST_DIALOG, NULL));
}

void errands_sidebar_rename_list_dialog_show(ErrandsSidebarTaskListRow *row) {
  if (!state.main_window->sidebar->rename_list_dialog)
    state.main_window->sidebar->rename_list_dialog = errands_sidebar_rename_list_dialog_new();
  state.main_window->sidebar->rename_list_dialog->current_task_list_row = row;
  gtk_editable_set_text(GTK_EDITABLE(state.main_window->sidebar->rename_list_dialog->entry),
                        errands_data_get_str(row->data, DATA_PROP_LIST_NAME));
  adw_dialog_present(ADW_DIALOG(state.main_window->sidebar->rename_list_dialog), GTK_WIDGET(state.main_window));
  gtk_widget_grab_focus(state.main_window->sidebar->rename_list_dialog->entry);
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsSidebarRenameListDialog *dialog, gchar *response, gpointer data) {
  if (g_str_equal(response, "rename")) {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(dialog->entry));
    LOG("SidebarRenameListDialog: Rename to '%s'", text);
    errands_data_set_str(dialog->current_task_list_row->data, DATA_PROP_LIST_NAME, text);
    errands_data_write_list(dialog->current_task_list_row->data);
    errands_sidebar_task_list_row_update_title(dialog->current_task_list_row);
    errands_task_list_update_title();
    // TODO: sync
  }
}

static void on_entry_changed_cb(AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  const char *list_name = errands_data_get_str(
      state.main_window->sidebar->rename_list_dialog->current_task_list_row->data, DATA_PROP_LIST_NAME);
  const bool enable = !g_str_equal("", text) && !g_str_equal(text, list_name);
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(state.main_window->sidebar->rename_list_dialog), "rename",
                                        enable);
}

static void on_entry_activated_cb(AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (g_str_equal(text, "")) return;
  on_response_cb(state.main_window->sidebar->rename_list_dialog, "rename", NULL);
  adw_dialog_close(ADW_DIALOG(state.main_window->sidebar->rename_list_dialog));
}
