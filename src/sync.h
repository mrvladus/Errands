// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include <stdbool.h>

#include "task-item.h"
#include "task-list-item.h"

void errands_sync_init(void);
void errands_sync(void);
void errands_sync_cleanup(void);
// Whether the first sync after startup has finished pulling the server's state
bool errands_sync_first_pull_done(void);

void errands_sync_push_list(ErrandsTaskListItem *list);
void errands_sync_push_task(ErrandsTaskItem *task);
