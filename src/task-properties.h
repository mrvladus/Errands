// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include "task-item.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_PROPERTIES (errands_task_properties_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskProperties, errands_task_properties, ERRANDS, TASK_PROPERTIES, AdwBin)

ErrandsTaskProperties *errands_task_properties_new();
void errands_task_properties_show_properties(ErrandsTaskProperties *self, ErrandsTaskItem *item);
void errands_task_properties_show_notes(ErrandsTaskProperties *self, ErrandsTaskItem *item);
void errands_task_properties_show_tags(ErrandsTaskProperties *self, ErrandsTaskItem *item);
void errands_task_properties_close(ErrandsTaskProperties *self);
