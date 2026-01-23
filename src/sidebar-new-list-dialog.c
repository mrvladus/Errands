#include "data.h"
#include "glib.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"
#include "window.h"

static void on_response_cb(ErrandsSidebarNewListDialog *self, gchar *response, gpointer data);
static void on_entry_changed_cb(ErrandsSidebarNewListDialog *self, AdwEntryRow *entry);
static void on_entry_activated_cb(ErrandsSidebarNewListDialog *self, AdwEntryRow *entry);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsSidebarNewListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
};

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

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_sidebar_new_list_dialog_show() {
  ErrandsSidebarNewListDialog *self = state.main_window->sidebar->new_list_dialog;
  if (!self) state.main_window->sidebar->new_list_dialog = self = errands_sidebar_new_list_dialog_new();
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
  gtk_editable_set_text(GTK_EDITABLE(self->entry), "");
  gtk_widget_grab_focus(self->entry);
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsSidebarNewListDialog *self, gchar *response, gpointer data) {
  if (STR_EQUAL(response, "create")) {
    ListData *list = errands_list_data_create(generate_uuid4(), gtk_editable_get_text(GTK_EDITABLE(self->entry)), NULL,
                                              generate_hex_as_str(), false, false);
    g_ptr_array_add(errands_data_lists, list);
    ErrandsSidebarTaskListRow *row = errands_sidebar_add_task_list(list);
    g_signal_emit_by_name(row, "activate", NULL);
    errands_window_update();
    LOG("SidebarNewListDialog: Create new list: '%s'", list->uid);
    errands_list_data_save(list);
    errands_sync_create_list(list);
  }
}

static void on_entry_changed_cb(ErrandsSidebarNewListDialog *self, AdwEntryRow *entry) {
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(self), "create",
                                        !STR_EQUAL("", gtk_editable_get_text(GTK_EDITABLE(entry))));
}

static void on_entry_activated_cb(ErrandsSidebarNewListDialog *self, AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (STR_EQUAL(text, "")) return;
  on_response_cb(self, "create", NULL);
  adw_dialog_close(ADW_DIALOG(self));
}
