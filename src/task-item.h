#pragma once

#include "data.h"

#include <gio/gio.h>
#include <glib-object.h>

#define ERRANDS_TYPE_TASK_ITEM (errands_task_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskItem, errands_task_item, ERRANDS, TASK_ITEM, GObject)

struct _ErrandsTaskItem {
  GObject parent_instance;
  TaskData *data;
  GListStore *model;
};

ErrandsTaskItem *errands_task_item_new(TaskData *data);
