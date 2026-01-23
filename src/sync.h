#pragma once

#include "data.h"

void errands_sync_init(void);
void errands_sync_cleanup(void);
bool errands_sync(void);

void errands_sync_delete_list(ListData *data);
void errands_sync_create_list(ListData *data);
void errands_sync_update_list(ListData *data);
