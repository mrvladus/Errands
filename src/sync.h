#pragma once

#include "data.h"

void errands_sync_init();
void errands_sync_cleanup();
void errands_sync_schedule();
void errands_sync_schedule_list(ErrandsData *data);
void errands_sync_schedule_task(ErrandsData *data);
