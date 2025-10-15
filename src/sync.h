#pragma once

#include "data.h"

extern bool needs_sync;

void sync_init();
void sync();
void sync_list(ListData *list);
void sync_task(TaskData *task);
