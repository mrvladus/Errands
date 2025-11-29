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
  size_t total = 0, completed = 0, trash = 0;
  errands_data_get_stats(&total, &completed, &trash);
  const char *num = tmp_str_printf("%zu", trash);
  gtk_label_set_label(GTK_LABEL(row->counter), trash > 0 ? num : "");
  const char *icon_name = trash > 0 ? "errands-trash-full-symbolic" : "errands-trash-symbolic";
  gtk_image_set_from_icon_name(GTK_IMAGE(row->icon), icon_name);
  LOG("Sidebar Trash Row: Update counter: %zu", trash);
}
