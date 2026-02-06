#pragma once

#include "task.h"

#include <adwaita.h>

typedef enum {
  ERRANDS_TASK_MENU_MODE_TASK,
  ERRANDS_TASK_MENU_MODE_ENTRY,
} ErrandsTaskMenuMode;

#define ERRANDS_TYPE_TASK_MENU (errands_task_menu_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskMenu, errands_task_menu, ERRANDS, TASK_MENU, GtkPopover)

ErrandsTaskMenu *errands_task_menu_new();
void errands_task_menu_show(ErrandsTask *task, float x, float y, ErrandsTaskMenuMode mode);
