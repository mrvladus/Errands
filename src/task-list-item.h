#pragma once

#include "data.h"

#include <gdk/gdk.h>

#define ERRANDS_TYPE_TASK_LIST_ITEM (errands_task_list_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListItem, errands_task_list_item, ERRANDS, TASK_LIST_ITEM, GObject)

struct _ErrandsTaskListItem {
  GObject parent_instance;

  const char *uid;
  const char *title;
  const char *count_string; // Number of uncompleted tasks as a string
  GdkRGBA color;

  ListData *data;
};

ErrandsTaskListItem *errands_task_list_item_new(ListData *data);
void errands_task_list_item_update_counter(ErrandsTaskListItem *self);
