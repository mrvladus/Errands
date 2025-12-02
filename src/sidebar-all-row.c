#include "sidebar.h"

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsSidebarAllRow, errands_sidebar_all_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_all_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_ALL_ROW);
  G_OBJECT_CLASS(errands_sidebar_all_row_parent_class)->dispose(gobject);
}

static void errands_sidebar_all_row_class_init(ErrandsSidebarAllRowClass *class) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-all-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarAllRow, counter);
  G_OBJECT_CLASS(class)->dispose = errands_sidebar_all_row_dispose;
}

static void errands_sidebar_all_row_init(ErrandsSidebarAllRow *self) {
  LOG("Sidebar All Row: Create");
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsSidebarAllRow *errands_sidebar_all_row_new() { return g_object_new(ERRANDS_TYPE_SIDEBAR_ALL_ROW, NULL); }
