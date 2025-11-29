#pragma once

#include "data.h"

void sync_init();
void sync();
void sync_list(ListData *list);
void sync_task(TaskData *task);

void errands_sync_schedule();
void errands_sync_schedule_list(ListData *data);
void errands_sync_schedule_task(TaskData *data);
