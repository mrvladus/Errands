#include "data.h"
#include "sidebar.h"
#include "vendor/toolbox.h"

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsSidebarPinnedRow {
  GtkListBoxRow parent_instance;
  GtkWidget *icon;
  GtkWidget *counter;
};

G_DEFINE_TYPE(ErrandsSidebarPinnedRow, errands_sidebar_pinned_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_pinned_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_PINNED_ROW);
  G_OBJECT_CLASS(errands_sidebar_pinned_row_parent_class)->dispose(gobject);
}

static void errands_sidebar_pinned_row_class_init(ErrandsSidebarPinnedRowClass *class) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-pinned-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarPinnedRow, icon);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarPinnedRow, counter);
  G_OBJECT_CLASS(class)->dispose = errands_sidebar_pinned_row_dispose;
}

static void errands_sidebar_pinned_row_init(ErrandsSidebarPinnedRow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsSidebarPinnedRow *errands_sidebar_pinned_row_new() {
  return g_object_new(ERRANDS_TYPE_SIDEBAR_PINNED_ROW, NULL);
}
