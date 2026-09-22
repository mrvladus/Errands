#include "new-list-dialog.h"
#include "data.h"
#include "gio/gio.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "task-list-item.h"

static void on_response_cb(ErrandsNewListDialog *self, gchar *response, gpointer data);
static void on_entry_changed_cb(ErrandsNewListDialog *self, AdwEntryRow *entry);
static void on_entry_activated_cb(ErrandsNewListDialog *self, AdwEntryRow *entry);

static ErrandsNewListDialog *self = NULL;

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
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/new-list-dialog.ui");
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
  g_message("New List Dialog: Open");
  if (!self) self = errands_new_list_dialog_new();
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
  gtk_editable_set_text(GTK_EDITABLE(self->entry), "");
  gtk_widget_grab_focus(self->entry);
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsNewListDialog *self, gchar *response, gpointer data) {
  if (g_str_equal(response, "create")) {
    g_autofree gchar *uid = g_uuid_string_random();
    g_autoptr(ErrandsTaskListItem) item =
        errands_task_list_item_create(uid, gtk_editable_get_text(GTK_EDITABLE(self->entry)), NULL, false, false);
    g_message("New List Dialog: Create new list: '%s'", item->uid);
    g_list_store_append(task_lists_model, item);
    errands_task_list_item_save(item);
    errands_sidebar_update_filter_rows();
    // errands_sync_create_list(item);
    errands_settings_set(SETTING_LAST_LIST_UID, (void *)item->uid);
    errands_sidebar_select_last_opened_page();
  }
}

static void on_entry_changed_cb(ErrandsNewListDialog *self, AdwEntryRow *entry) {
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(self), "create",
                                        !g_str_equal("", gtk_editable_get_text(GTK_EDITABLE(entry))));
}

static void on_entry_activated_cb(ErrandsNewListDialog *self, AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (g_str_equal(text, "")) return;
  on_response_cb(self, "create", NULL);
  adw_dialog_close(ADW_DIALOG(self));
}
