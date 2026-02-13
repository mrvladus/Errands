#include "task-item.h"
#include "glib-object.h"

struct _ErrandsTaskItem {
  GObject parent_instance;
  TaskData *data;
  GListStore *children_model;
};

G_DEFINE_TYPE(ErrandsTaskItem, errands_task_item, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_TASK_DATA,
  PROP_CHILDREN_MODEL,
  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

static void errands_task_item_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);
  switch (prop_id) {
  case PROP_TASK_DATA: self->data = g_value_get_pointer(value); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void errands_task_item_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);
  switch (prop_id) {
  case PROP_TASK_DATA: g_value_set_pointer(value, self->data); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void errands_task_item_dispose(GObject *object) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);

  if (self->children_model) g_object_unref(self->children_model);

  G_OBJECT_CLASS(errands_task_item_parent_class)->dispose(object);
}

static void errands_task_item_class_init(ErrandsTaskItemClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = errands_task_item_dispose;

  object_class->set_property = errands_task_item_set_property;
  object_class->get_property = errands_task_item_get_property;

  obj_properties[PROP_TASK_DATA] =
      g_param_spec_pointer("task-data", "Task Data", "Data associated with the task.", G_PARAM_READWRITE);
  obj_properties[PROP_CHILDREN_MODEL] = g_param_spec_object(
      "children-model", "Children Model", "Model containing child tasks.", G_TYPE_LIST_MODEL, G_PARAM_READWRITE);
  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void errands_task_item_init(ErrandsTaskItem *self) {}

ErrandsTaskItem *errands_task_item_new(TaskData *data) {
  ErrandsTaskItem *self = g_object_new(ERRANDS_TYPE_TASK_ITEM, NULL);
  self->data = data;
  self->children_model = NULL;
  return self;
}

GListModel *errands_task_item_get_children_model(ErrandsTaskItem *self) {
  if (self->children_model) return G_LIST_MODEL(self->children_model);

  self->children_model = g_list_store_new(ERRANDS_TYPE_TASK_ITEM);

  for_range(i, 0, self->data->children->len) {
    TaskData *child = g_ptr_array_index(self->data->children, i);
    g_autoptr(ErrandsTaskItem) item = errands_task_item_new(child);
    g_list_store_append(self->children_model, item);
  }

  return G_LIST_MODEL(self->children_model);
}

TaskData *errands_task_item_get_task_data(ErrandsTaskItem *self) { return self->data; }
