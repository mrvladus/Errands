#pragma once

#include <gio/gio.h>

#define ERRANDS_TYPE_TASK_LIST_ITEM (errands_task_list_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListItem, errands_task_list_item, ERRANDS, TASK_LIST_ITEM, GObject)

ErrandsTaskListItem *errands_task_list_item_new();
// TaskData *errands_task_list_item_get_data(ErrandsTaskListItem *self);
// GListModel *errands_task_list_item_get_children_model(ErrandsTaskItem *self);
// ErrandsTaskItem *errands_task_list_item_get_parent(ErrandsTaskItem *self);
// void errands_task_list_item_set_parent(ErrandsTaskItem *self, ErrandsTaskItem *parent);
// ErrandsTaskItem *errands_task_list_item_add_child(ErrandsTaskItem *self, TaskData *data);
