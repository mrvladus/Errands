#include "rename-list-dialog.h"
#include "config.h"
#include "state.h"

static void on_response_cb(ErrandsRenameListDialog *self, gchar *response, gpointer data);
static void on_entry_changed_cb(ErrandsRenameListDialog *self, AdwEntryRow *entry);
static void on_entry_activated_cb(ErrandsRenameListDialog *self, AdwEntryRow *entry);

static ErrandsRenameListDialog *self = NULL;

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsRenameListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
  ErrandsTaskListItem *item;
};

G_DEFINE_TYPE(ErrandsRenameListDialog, errands_rename_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void errands_rename_list_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_RENAME_LIST_DIALOG);
  G_OBJECT_CLASS(errands_rename_list_dialog_parent_class)->dispose(gobject);
}

static void errands_rename_list_dialog_class_init(ErrandsRenameListDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_rename_list_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/rename-list-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsRenameListDialog, entry);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_response_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_activated_cb);
}

static void errands_rename_list_dialog_init(ErrandsRenameListDialog *dialog) {
  gtk_widget_init_template(GTK_WIDGET(dialog));
}

ErrandsRenameListDialog *errands_rename_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_RENAME_LIST_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_rename_list_dialog_show(ErrandsTaskListItem *item) {
  g_message("Rename List Dialog: Show");
  if (!self) self = errands_rename_list_dialog_new();
  self->item = item;
  gtk_editable_set_text(GTK_EDITABLE(self->entry), item->title);
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
  gtk_widget_grab_focus(self->entry);
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsRenameListDialog *self, gchar *response, gpointer data) {
  if (!g_str_equal(response, "rename")) return;
  g_object_set(self->item, "title", gtk_editable_get_text(GTK_EDITABLE(self->entry)), NULL);
  errands_task_list_update(state.main_window->task_list);
}

static void on_entry_changed_cb(ErrandsRenameListDialog *self, AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  const bool enable = !g_str_equal("", text) && !g_str_equal(text, self->item->title);
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(self), "rename", enable);
}

static void on_entry_activated_cb(ErrandsRenameListDialog *self, AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (g_str_equal(text, "")) return;
  on_response_cb(self, "rename", NULL);
  adw_dialog_close(ADW_DIALOG(self));
}
