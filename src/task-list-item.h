#pragma once

#include "data.h"

#include <gdk/gdk.h>

#define ERRANDS_TYPE_TASK_LIST_ITEM (errands_task_list_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListItem, errands_task_list_item, ERRANDS, TASK_LIST_ITEM, GObject)

struct _ErrandsTaskListItem {
  GObject parent_instance;

  const char *uid;
  const char *title;
  GdkRGBA color;

  ListData *data;
};

ErrandsTaskListItem *errands_task_list_item_new(ListData *data);
