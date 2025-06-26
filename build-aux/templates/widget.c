#include "widget.h"

static void child_cb(GtkWidget *child) {}

G_DEFINE_TYPE(ErrandsWidget, errands_widget, ADW_TYPE_BIN)

static void errands_widget_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_WIDGET);
  G_OBJECT_CLASS(errands_widget_parent_class)->dispose(gobject);
}

static void errands_widget_class_init(ErrandsWidgetClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_widget_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/io/github/mrvladus/Errands/ui/widget.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWidget, child);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), child_cb);
}

static void errands_widget_init(ErrandsWidget *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsWidget *errands_widget_new() { return g_object_new(ERRANDS_TYPE_WIDGET, NULL); }
