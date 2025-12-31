#pragma once

#include "data.h"

void errands_sync_init(void);
void errands_sync_cleanup(void);
void errands_sync_schedule(void);
void errands_sync_schedule_list(ListData *data);
void errands_sync_schedule_task(TaskData *data);
