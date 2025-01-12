#pragma once

#include <gtk/gtk.h>
#include <libical/ical.h>
#include <stddef.h>

typedef union {
  const char *s;
  size_t i;
  bool b;
} ErrandsDataVal;

typedef icalcomponent ListData;

typedef enum {
  LIST_PROP_COLOR,
  LIST_PROP_DELETED,
  LIST_PROP_NAME,
  LIST_PROP_SYNCED,
  LIST_PROP_UID,
} ListDataProp;

ListData *list_data_new(const char *uid);
void list_data_free(ListData *data);
ErrandsDataVal list_data_get(ListData *data, ListDataProp prop);
void list_data_set(ListData *data, ListDataProp prop, void *value);

typedef icalcomponent TaskData;

typedef enum {
  TASK_PROP_ATTACHMENTS,
  TASK_PROP_COLOR,
  TASK_PROP_COMPLETED,
  TASK_PROP_CHANGED,
  TASK_PROP_CREATED,
  TASK_PROP_DELETED,
  TASK_PROP_DUE,
  TASK_PROP_EXPANDED,
  TASK_PROP_LIST_UID,
  TASK_PROP_NOTES,
  TASK_PROP_NOTIFIED,
  TASK_PROP_PARENT,
  TASK_PROP_PRIORITY,
  TASK_PROP_RRULE,
  TASK_PROP_START,
  TASK_PROP_SYNCED,
  TASK_PROP_TAGS,
  TASK_PROP_TEXT,
  TASK_PROP_TOOLBAR_SHOWN,
  TASK_PROP_TRASH,
  TASK_PROP_UID,
} TaskDataProp;

TaskData *task_data_new(ListData *list, const char *text, const char *parent);
void task_data_free(TaskData *data);
ErrandsDataVal task_data_get(ListData *data, TaskDataProp prop);
void task_data_set(TaskData *data, TaskDataProp prop, void *value);

GPtrArray *errands_data_load_lists();
GPtrArray *errands_data_load_tasks(GPtrArray *calendars);
