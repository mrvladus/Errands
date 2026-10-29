// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include "task-item.h"
#include "task-list-item.h"

void errands_sync_init(void);
void errands_sync(void);
void errands_sync_cleanup(void);

void errands_sync_push_list(ErrandsTaskListItem *list);
void errands_sync_push_task(ErrandsTaskItem *task);
