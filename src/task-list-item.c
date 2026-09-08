#include "task-list-item.h"
#include "data.h"
#include "utils.h"

G_DEFINE_TYPE(ErrandsTaskListItem, errands_task_list_item, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_UID,
  PROP_TITLE,
  PROP_COLOR,
  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

static void errands_task_list_item_set_property(GObject *object, guint prop_id, const GValue *value,
                                                GParamSpec *pspec) {
  ErrandsTaskListItem *self = ERRANDS_TASK_LIST_ITEM(object);
  switch (prop_id) {
  case PROP_UID: {
    self->uid = g_value_get_string(value);
    errands_data_set_uid(self->data->ical, self->uid);
    errands_list_data_save(self->data);
  } break;
  case PROP_TITLE: {
    self->title = g_value_get_string(value);
    errands_data_set_list_name(self->data->ical, self->title);
    errands_list_data_save(self->data);
  } break;
  case PROP_COLOR: {
    GdkRGBA *color = g_value_get_boxed(value);
    if (color) self->color = *color;
    char hex_string[8];
    gdk_rgba_to_hex_string(&self->color, hex_string);
    errands_data_set_color(self->data->ical, hex_string, true);
    errands_list_data_save(self->data);
  } break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void errands_task_list_item_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsTaskListItem *self = ERRANDS_TASK_LIST_ITEM(object);
  switch (prop_id) {
  case PROP_UID: {
    g_value_set_string(value, self->uid);
  } break;
  case PROP_TITLE: {
    g_value_set_string(value, self->title);
  } break;
  case PROP_COLOR: {
    g_value_set_boxed(value, &self->color);
  } break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void errands_task_list_item_dispose(GObject *object) {
  ErrandsTaskListItem *self = ERRANDS_TASK_LIST_ITEM(object);

  // if (self->children_model) g_object_unref(self->children_model);

  G_OBJECT_CLASS(errands_task_list_item_parent_class)->dispose(object);
}

static void errands_task_list_item_class_init(ErrandsTaskListItemClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = errands_task_list_item_dispose;

  object_class->set_property = errands_task_list_item_set_property;
  object_class->get_property = errands_task_list_item_get_property;

  obj_properties[PROP_UID] =
      g_param_spec_pointer("uid", "Task List UID", "Unique identifier for the task list", G_PARAM_READWRITE);
  obj_properties[PROP_TITLE] =
      g_param_spec_string("title", "Task List Title", "Title of the task list", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_COLOR] =
      g_param_spec_boxed("color", "Task List Color", "Color of the task list", GDK_TYPE_RGBA, G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void errands_task_list_item_init(ErrandsTaskListItem *self) {}

ErrandsTaskListItem *errands_task_list_item_new(ListData *data) {
  ErrandsTaskListItem *self = g_object_new(ERRANDS_TYPE_TASK_LIST_ITEM, NULL);
  self->data = data;
  self->uid = errands_data_get_uid(data->ical);
  self->title = errands_data_get_list_name(data->ical);
  gdk_rgba_parse(&self->color, errands_data_get_color(data->ical, true));
  return self;
}
