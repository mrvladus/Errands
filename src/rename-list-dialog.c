// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "rename-list-dialog.h"
#include "config.h"
#include "state.h"
#include "sync.h"
#include "task-list-item.h"

static void on_response_cb(ErrandsRenameListDialog *self, gchar *response, gpointer data);
static void on_entry_changed_cb(ErrandsRenameListDialog *self, AdwEntryRow *entry);
static void on_entry_activated_cb(ErrandsRenameListDialog *self, AdwEntryRow *entry);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsRenameListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
  ErrandsTaskListItem *item;
};

G_DEFINE_TYPE(ErrandsRenameListDialog, errands_rename_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void finalize(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_RENAME_LIST_DIALOG);
  G_OBJECT_CLASS(errands_rename_list_dialog_parent_class)->finalize(gobject);
}

static void errands_rename_list_dialog_class_init(ErrandsRenameListDialogClass *class) {
  G_OBJECT_CLASS(class)->finalize = finalize;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/rename-list-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsRenameListDialog, entry);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_response_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_activated_cb);
}

static void errands_rename_list_dialog_init(ErrandsRenameListDialog *dialog) { gtk_widget_init_template(GTK_WIDGET(dialog)); }

ErrandsRenameListDialog *errands_rename_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_RENAME_LIST_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_rename_list_dialog_show(ErrandsTaskListItem *item) {
  g_message("Rename List Dialog: Show");
  if (!state.rename_list_dialog) state.rename_list_dialog = errands_rename_list_dialog_new();
  state.rename_list_dialog->item = item;
  gtk_editable_set_text(GTK_EDITABLE(state.rename_list_dialog->entry), item->title);
  adw_dialog_present(ADW_DIALOG(state.rename_list_dialog), GTK_WIDGET(state.main_window));
  gtk_widget_grab_focus(state.rename_list_dialog->entry);
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsRenameListDialog *self, gchar *response, gpointer data) {
  if (!g_str_equal(response, "rename")) return;
  errands_task_list_item_set_title(self->item, gtk_editable_get_text(GTK_EDITABLE(self->entry)));
  errands_task_list_update(state.main_window->task_list);
  errands_sync_update_list(self->item);
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
