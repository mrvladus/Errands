#pragma once

#include "data.h"

#include <gdk/gdk.h>

#define ERRANDS_TYPE_TASK_ITEM (errands_task_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskItem, errands_task_item, ERRANDS, TASK_ITEM, GObject)

ErrandsTaskItem *errands_task_item_new(TaskData *data, ErrandsTaskItem *parent);
ErrandsTaskItem *errands_task_item_add_child(ErrandsTaskItem *self, TaskData *data);

void errands_task_item_update_sub_task_count(ErrandsTaskItem *self);

const char *errands_task_item_get_color(ErrandsTaskItem *self);
ErrandsTaskItem *errands_task_item_get_parent(ErrandsTaskItem *self);
GListModel *errands_task_item_get_children_model(ErrandsTaskItem *self);
TaskData *errands_task_item_get_data(ErrandsTaskItem *self);

void errands_task_item_set_color(ErrandsTaskItem *self, const char *color);
void errands_task_item_set_parent(ErrandsTaskItem *self, ErrandsTaskItem *parent);
