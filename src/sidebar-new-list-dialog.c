#include "sidebar-new-list-dialog.h"
#include "state.h"
#include "utils.h"

static void on_response_cb(ErrandsSidebarNewListDialog *dialog, gchar *response, gpointer data);
static void on_entry_changed_cb(AdwEntryRow *entry);
static void on_entry_activated_cb(AdwEntryRow *entry);

G_DEFINE_TYPE(ErrandsSidebarNewListDialog, errands_sidebar_new_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void errands_errands_sidebar_new_list_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_NEW_LIST_DIALOG);
  G_OBJECT_CLASS(errands_sidebar_new_list_dialog_parent_class)->dispose(gobject);
}

static void errands_sidebar_new_list_dialog_class_init(ErrandsSidebarNewListDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_errands_sidebar_new_list_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-new-list-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarNewListDialog, entry);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_response_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_activated_cb);
}

static void errands_sidebar_new_list_dialog_init(ErrandsSidebarNewListDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsSidebarNewListDialog *errands_sidebar_new_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_SIDEBAR_NEW_LIST_DIALOG, NULL));
}

void errands_sidebar_new_list_dialog_show() {
  if (!state.main_window->sidebar->new_list_dialog)
    state.main_window->sidebar->new_list_dialog = errands_sidebar_new_list_dialog_new();
  adw_dialog_present(ADW_DIALOG(state.main_window->sidebar->new_list_dialog), GTK_WIDGET(state.main_window));
  gtk_editable_set_text(GTK_EDITABLE(state.main_window->sidebar->new_list_dialog->entry), "");
  gtk_widget_grab_focus(state.main_window->sidebar->new_list_dialog->entry);
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsSidebarNewListDialog *dialog, gchar *response, gpointer data) {
  if (g_str_equal(response, "create")) {
    ListData *tld = list_data_new(NULL, gtk_editable_get_text(GTK_EDITABLE(dialog->entry)), NULL, false, false,
                                  g_hash_table_size(ldata));
    g_hash_table_insert(ldata, strdup(errands_data_get_str(tld, DATA_PROP_LIST_UID)), tld);
    errands_data_write_list(tld);
    ErrandsSidebarTaskListRow *row = errands_sidebar_add_task_list(state.main_window->sidebar, tld);
    g_signal_emit_by_name(row, "activate", NULL);
    g_object_set(state.main_window->no_lists_page, "visible", false, NULL);
    LOG("SidebarNewListDialog: Create new list: '%s'", errands_data_get_str(tld, DATA_PROP_LIST_UID));
  }
}

static void on_entry_changed_cb(AdwEntryRow *entry) {
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(state.main_window->sidebar->new_list_dialog), "create",
                                        !g_str_equal("", gtk_editable_get_text(GTK_EDITABLE(entry))));
}

static void on_entry_activated_cb(AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (g_str_equal(text, "")) return;
  on_response_cb(state.main_window->sidebar->new_list_dialog, "create", NULL);
  adw_dialog_close(ADW_DIALOG(state.main_window->sidebar->new_list_dialog));
}
