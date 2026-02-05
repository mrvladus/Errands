#include "new-list-dialog.h"
#include "data.h"
#include "state.h"
#include "sync.h"

static void on_response_cb(ErrandsNewListDialog *self, gchar *response, gpointer data);
static void on_entry_changed_cb(ErrandsNewListDialog *self, AdwEntryRow *entry);
static void on_entry_activated_cb(ErrandsNewListDialog *self, AdwEntryRow *entry);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsNewListDialog {
  AdwAlertDialog parent_instance;
  GtkWidget *entry;
};

G_DEFINE_TYPE(ErrandsNewListDialog, errands_new_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void errands_new_list_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_NEW_LIST_DIALOG);
  G_OBJECT_CLASS(errands_new_list_dialog_parent_class)->dispose(gobject);
}

static void errands_new_list_dialog_class_init(ErrandsNewListDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_new_list_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/new-list-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsNewListDialog, entry);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_response_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_activated_cb);
}

static void errands_new_list_dialog_init(ErrandsNewListDialog *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsNewListDialog *errands_new_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_NEW_LIST_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_new_list_dialog_show() {
  ErrandsNewListDialog *self = state.main_window->sidebar->new_list_dialog;
  if (!self) state.main_window->sidebar->new_list_dialog = self = errands_new_list_dialog_new();
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
  gtk_editable_set_text(GTK_EDITABLE(self->entry), "");
  gtk_widget_grab_focus(self->entry);
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsNewListDialog *self, gchar *response, gpointer data) {
  if (STR_EQUAL(response, "create")) {
    ListData *list = errands_list_data_create(generate_uuid4(), gtk_editable_get_text(GTK_EDITABLE(self->entry)), NULL,
                                              generate_hex_as_str(), false, false);
    LOG("New List Dialog: Create new list: '%s'", list->uid);
    errands_list_data_save(list);
    g_ptr_array_add(errands_data_lists, list);
    errands_sidebar_load_lists();
    ErrandsSidebarTaskListRow *row = errands_sidebar_find_row(list);
    if (row) g_signal_emit_by_name(row, "activate", NULL);
    errands_sidebar_update_filter_rows();
    errands_sync_create_list(list);
  }
}

static void on_entry_changed_cb(ErrandsNewListDialog *self, AdwEntryRow *entry) {
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(self), "create",
                                        !STR_EQUAL("", gtk_editable_get_text(GTK_EDITABLE(entry))));
}

static void on_entry_activated_cb(ErrandsNewListDialog *self, AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (STR_EQUAL(text, "")) return;
  on_response_cb(self, "create", NULL);
  adw_dialog_close(ADW_DIALOG(self));
}
