#pragma once

#include "data.h"

#include <gio/gio.h>

#define ERRANDS_TYPE_TASK_ITEM (errands_task_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskItem, errands_task_item, ERRANDS, TASK_ITEM, GObject)

ErrandsTaskItem *errands_task_item_new(TaskData *data, ErrandsTaskItem *parent);
TaskData *errands_task_item_get_data(ErrandsTaskItem *self);
GListModel *errands_task_item_get_children_model(ErrandsTaskItem *self);
ErrandsTaskItem *errands_task_item_get_parent(ErrandsTaskItem *self);
void errands_task_item_set_parent(ErrandsTaskItem *self, ErrandsTaskItem *parent);
void errands_task_item_add_child(ErrandsTaskItem *self, TaskData *data);
