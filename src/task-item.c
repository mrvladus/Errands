#include "task-item.h"

struct _ErrandsTaskItem {
  GObject parent_instance;
  TaskData *data;
  GListStore *children_model;
};

G_DEFINE_TYPE(ErrandsTaskItem, errands_task_item, G_TYPE_OBJECT)

static void errands_task_item_dispose(GObject *object) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);

  if (self->children_model) g_object_unref(self->children_model);

  G_OBJECT_CLASS(errands_task_item_parent_class)->dispose(object);
}

static void errands_task_item_class_init(ErrandsTaskItemClass *klass) {
  G_OBJECT_CLASS(klass)->dispose = errands_task_item_dispose;
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
