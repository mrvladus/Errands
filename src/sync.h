#pragma once

#include "data.h"

void sync_init();
void sync();
void sync_list(ListData2 *list);
void sync_task(TaskData2 *task);

void errands_sync_schedule();
void errands_sync_schedule_list(ListData2 *data);
void errands_sync_schedule_task(TaskData2 *data);
