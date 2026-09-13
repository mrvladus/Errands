#include "widget.h"
#include "config.h"

static void child_cb(GtkWidget *child);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsWidget {
  AdwBin parent_instance;
  GtkWidget *child;

  // Properties
  gboolean bool_prop;
};

G_DEFINE_TYPE(ErrandsWidget, errands_widget, ADW_TYPE_BIN)

enum {
  PROP_0,

  PROP_BOOL_PROP,

  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsWidget *self = ERRANDS_WIDGET(object);
  switch (prop_id) {
  case PROP_BOOL_PROP: g_value_set_boolean(value, self->bool_prop); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsWidget *self = ERRANDS_WIDGET(object);
  switch (prop_id) {
  case PROP_BOOL_PROP: self->bool_prop = g_value_get_boolean(value); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void errands_widget_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_WIDGET);
  G_OBJECT_CLASS(errands_widget_parent_class)->dispose(gobject);
}

static void errands_widget_class_init(ErrandsWidgetClass *class) {
  GObjectClass *object_class = G_OBJECT_CLASS(class);
  object_class->dispose = errands_widget_dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  obj_properties[PROP_BOOL_PROP] =
      g_param_spec_boolean("bool-prop", "Bool Prop", "This is bool prop", false, G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/widget.ui");

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWidget, child);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), child_cb);
}

static void errands_widget_init(ErrandsWidget *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsWidget *errands_widget_new() { return g_object_new(ERRANDS_TYPE_WIDGET, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

// ---------- CALLBACKS ---------- //

static void child_cb(GtkWidget *child) {}
