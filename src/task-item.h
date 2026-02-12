#pragma once

#include "data.h"

#include <gio/gio.h>
#include <glib-object.h>

#define ERRANDS_TYPE_TASK_ITEM (errands_task_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskItem, errands_task_item, ERRANDS, TASK_ITEM, GObject)

ErrandsTaskItem *errands_task_item_new(TaskData *data);
TaskData *errands_task_item_get_task_data(ErrandsTaskItem *self);
GListModel *errands_task_item_get_children_model(ErrandsTaskItem *self);
