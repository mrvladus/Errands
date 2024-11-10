#include "new-list-dialog.h"
#include "../data.h"
#include "../sidebar/sidebar-task-list-row.h"
#include "../sidebar/sidebar.h"
#include "../state.h"

#include <glib/gi18n.h>

#include <string.h>

// --- DECLARATIONS --- //

static void on_entry_changed_cb(AdwEntryRow *entry);
static void on_entry_activate_cb(AdwEntryRow *entry);
static void on_response_cb(ErrandsNewListDialog *dialog, gchar *response, gpointer data);

// --- IMPLEMENTATIONS --- //

G_DEFINE_TYPE(ErrandsNewListDialog, errands_new_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void errands_new_list_dialog_class_init(ErrandsNewListDialogClass *class) {}

static void errands_new_list_dialog_init(ErrandsNewListDialog *self) {
  adw_alert_dialog_set_heading(ADW_ALERT_DIALOG(self), _("Add Task List"));
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(self), "cancel", _("Cancel"));
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(self), "create", _("Create"));
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(self), "create",
                                           ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(self), "create");
  adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(self), "cancel");
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(self), "create", false);
  g_signal_connect(self, "response", G_CALLBACK(on_response_cb), NULL);

  // Box
  GtkWidget *box = gtk_list_box_new();
  g_object_set(box, "selection-mode", GTK_SELECTION_NONE, NULL);
  gtk_widget_add_css_class(box, "boxed-list");
  adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(self), box);

  // Entry
  self->entry = adw_entry_row_new();
  g_object_set(self->entry, "title", _("List Name"), "max-length", 255, NULL);
  g_signal_connect(self->entry, "changed", G_CALLBACK(on_entry_changed_cb), NULL);
  g_signal_connect(self->entry, "entry-activated", G_CALLBACK(on_entry_activate_cb), NULL);
  gtk_list_box_append(GTK_LIST_BOX(box), self->entry);
}

ErrandsNewListDialog *errands_new_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_NEW_LIST_DIALOG, NULL));
}

void errands_new_list_dialog_show() {
  adw_dialog_present(ADW_DIALOG(state.new_list_dialog), GTK_WIDGET(state.main_window));
  gtk_editable_set_text(GTK_EDITABLE(state.new_list_dialog->entry), "");
  gtk_widget_grab_focus(state.new_list_dialog->entry);
}

// --- SIGNAL HANDLERS --- //

static void on_entry_changed_cb(AdwEntryRow *entry) {
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(state.new_list_dialog), "create",
                                        strcmp("", gtk_editable_get_text(GTK_EDITABLE(entry))));
}

static void on_entry_activate_cb(AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (!strcmp(text, ""))
    return;
  on_response_cb(state.new_list_dialog, "create", NULL);
  adw_dialog_close(ADW_DIALOG(state.new_list_dialog));
}

static void on_response_cb(ErrandsNewListDialog *dialog, gchar *response, gpointer data) {
  if (!strcmp(response, "create")) {
    TaskListData *tld = errands_data_add_list(gtk_editable_get_text(GTK_EDITABLE(dialog->entry)));
    ErrandsSidebarTaskListRow *row = errands_sidebar_add_task_list(state.sidebar, tld);
    g_signal_emit_by_name(row, "activate", NULL);
    g_object_set(state.main_window->no_lists_page, "visible", false, NULL);
  }
}
