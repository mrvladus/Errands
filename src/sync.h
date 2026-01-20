#pragma once

// #include "data.h"

#include <glib.h>

extern GPtrArray *lists_to_delete_on_server;

void errands_sync_init(void);
void errands_sync_cleanup(void);
void errands_sync(void);
// void errands_sync_schedule_list(ListData *data);
// void errands_sync_schedule_task(TaskData *data);
