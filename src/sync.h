#pragma once

#include "task-item.h"
#include "task-list-item.h"

void errands_sync_init(void);
void errands_sync_cleanup(void);
bool errands_sync(void);

void errands_sync_delete_list(ErrandsTaskListItem *list);
void errands_sync_create_list(ErrandsTaskListItem *list);
void errands_sync_update_list(ErrandsTaskListItem *list);

void errands_sync_delete_task(ErrandsTaskItem *task);
void errands_sync_create_task(ErrandsTaskItem *task);
void errands_sync_update_task(ErrandsTaskItem *task);
