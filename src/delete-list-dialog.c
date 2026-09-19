#include "delete-list-dialog.h"
#include "config.h"
#include "data.h"
#include "sidebar.h"
#include "state.h"

static void on_response_cb(ErrandsDeleteListDialog *dialog, gchar *response, gpointer data);

static ErrandsDeleteListDialog *self = NULL;

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsDeleteListDialog {
  AdwAlertDialog parent_instance;
  ErrandsTaskListItem *item;
};

G_DEFINE_TYPE(ErrandsDeleteListDialog, errands_delete_list_dialog, ADW_TYPE_ALERT_DIALOG)

static void errands_delete_list_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_DELETE_LIST_DIALOG);
  G_OBJECT_CLASS(errands_delete_list_dialog_parent_class)->dispose(gobject);
}

static void errands_delete_list_dialog_class_init(ErrandsDeleteListDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_delete_list_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/delete-list-dialog.ui");
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_response_cb);
}

static void errands_delete_list_dialog_init(ErrandsDeleteListDialog *dialog) {
  gtk_widget_init_template(GTK_WIDGET(dialog));
}

ErrandsDeleteListDialog *errands_delete_list_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_DELETE_LIST_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_delete_list_dialog_show(ErrandsTaskListItem *item) {
  if (!self) self = errands_delete_list_dialog_new();
  self->item = item;
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
}

// ---------- CALLBACKS ---------- //

static void on_response_cb(ErrandsDeleteListDialog *dialog, gchar *response, gpointer data) {
  if (STR_EQUAL(response, "delete")) errands_sidebar_delete_list(dialog->item->uid);
}
