#include "data.h"
#include "gtk/gtk.h"
#include "sidebar.h"
#include "vendor/toolbox.h"

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsSidebarTrashRow {
  GtkListBoxRow parent_instance;
  GtkWidget *icon;
  GtkWidget *counter;
};

G_DEFINE_TYPE(ErrandsSidebarTrashRow, errands_sidebar_trash_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_trash_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_TRASH_ROW);
  G_OBJECT_CLASS(errands_sidebar_trash_row_parent_class)->dispose(gobject);
}

static void errands_sidebar_trash_row_class_init(ErrandsSidebarTrashRowClass *class) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-trash-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarTrashRow, icon);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarTrashRow, counter);
  G_OBJECT_CLASS(class)->dispose = errands_sidebar_trash_row_dispose;
}

static void errands_sidebar_trash_row_init(ErrandsSidebarTrashRow *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsSidebarTrashRow *errands_sidebar_trash_row_new() { return g_object_new(ERRANDS_TYPE_SIDEBAR_TRASH_ROW, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_sidebar_trash_row_update(ErrandsSidebarTrashRow *row) {
  // GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  // size_t len = 0;
  // for (size_t i = 0; i < tasks->len; i++) {
  //   TaskData *td = tasks->pdata[i];
  //   bool deleted = errands_data_get_bool(td, DATA_PROP_DELETED);
  //   bool trash = errands_data_get_bool(td, DATA_PROP_TRASH);
  //   if (!deleted && trash) len++;
  // }
  // g_ptr_array_free(tasks, false);
  // char num[64];
  // g_snprintf(num, 64, "%zu", len);
  // gtk_label_set_label(GTK_LABEL(row->counter), len > 0 ? num : "");
  // const char *icon_name = len > 0 ? "errands-trash-full-symbolic" : "errands-trash-symbolic";
  // gtk_image_set_from_icon_name(GTK_IMAGE(row->icon), icon_name);
}
