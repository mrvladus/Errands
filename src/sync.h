#pragma once

#include "data.h"

void errands_sync_init(void);
void errands_sync_cleanup(void);
bool errands_sync(void);
void errands_sync_push_list(ListData *data);
// void errands_sync_schedule_task(TaskData *data);
