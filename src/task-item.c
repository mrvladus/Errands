#include "task-item.h"

G_DEFINE_TYPE(ErrandsTaskItem, errands_task_item, G_TYPE_OBJECT)
static void errands_task_item_class_init(ErrandsTaskItemClass *klass) {}
static void errands_task_item_init(ErrandsTaskItem *self) {}

ErrandsTaskItem *errands_task_item_new(TaskData *data) {
  ErrandsTaskItem *item = g_object_new(ERRANDS_TYPE_TASK_ITEM, NULL);
  item->data = data;
  item->model = g_list_store_new(ERRANDS_TYPE_TASK_ITEM);
  for (int i = 0; i < data->children->len; i++) {
    g_autoptr(ErrandsTaskItem) child = errands_task_item_new(data);
    g_list_store_append(item->model, child);
  }

  return item;
}
