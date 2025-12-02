#include "sidebar.h"

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsSidebarTodayRow, errands_sidebar_today_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_today_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_TODAY_ROW);
  G_OBJECT_CLASS(errands_sidebar_today_row_parent_class)->dispose(gobject);
}

static void errands_sidebar_today_row_class_init(ErrandsSidebarTodayRowClass *class) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-today-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarTodayRow, counter);
  G_OBJECT_CLASS(class)->dispose = errands_sidebar_today_row_dispose;
}

static void errands_sidebar_today_row_init(ErrandsSidebarTodayRow *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsSidebarTodayRow *errands_sidebar_today_row_new() { return g_object_new(ERRANDS_TYPE_SIDEBAR_TODAY_ROW, NULL); }
