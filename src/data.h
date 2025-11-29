#pragma once

#include "vendor/toolbox.h"

#include <glib-object.h>
#include <libical/ical.h>

#include <stdbool.h>
#include <stddef.h>

typedef struct TaskData TaskData;

extern GPtrArray *errands_data_lists;

// Get all tasks as flat list
void errands_data_get_flat_list(GPtrArray *tasks);
// Get the total number of tasks and completed tasks
void errands_data_get_stats(size_t *total, size_t *completed, size_t *trash);
void errands_data_sort();

// --- LIST DATA --- //

typedef struct {
  icalcomponent *data;
  GPtrArray *children;
} ListData;

#define LIST_DATA(ptr) ((ListData *)(ptr))

ListData *errands_list_data_new(icalcomponent *data);
void errands_list_data_free(ListData *data);
AUTOPTR_DEFINE(ListData, errands_list_data_free)
// Create new `ListData`.
// `uid` - Pass `NULL` to generate a new UID.
// `color` - Pass `NULL` to generate color.
ListData *errands_list_data_create(const char *uid, const char *name, const char *color, bool deleted, bool synced);
void errands_list_data_sort(ListData *data);
// Get all tasks as flat list
void errands_list_data_get_flat_list(ListData *data, GPtrArray *tasks);
void errands_list_data_get_stats(ListData *data, size_t *total, size_t *completed, size_t *trash);
GPtrArray *errands_list_data_get_all_tasks_as_icalcomponents(ListData *data);
void errands_list_data_print(ListData *data);

// --- TASK DATA --- //

struct TaskData {
  icalcomponent *data;
  GPtrArray *children;
  TaskData *parent;
  ListData *list;
};

#define TASK_DATA(ptr) ((TaskData *)(ptr))

TaskData *errands_task_data_new(icalcomponent *data, TaskData *parent, ListData *list);
TaskData *errands_task_data_create_task(ListData *list, TaskData *parent, const char *text);
void errands_task_data_free(TaskData *data);
size_t errands_task_data_get_indent_level(TaskData *data);
// Get the total number of sub-tasks and completed tasks
void errands_task_data_get_stats_recursive(TaskData *data, size_t *total, size_t *completed, size_t *trash);
void errands_task_data_get_flat_list(TaskData *parent, GPtrArray *array);
void errands_task_data_print(TaskData *data);

// --- LOAD & SAVE & CLEANUP --- //

void errands_data_init(void);
void errands_data_write_list(ListData *data);
void errands_data_cleanup(void);

// --- SORT AND FILTER FUNCTIONS --- //

gint errands_data_sort_func(gconstpointer a, gconstpointer b);

// --- ICAL UTILS --- //

bool icaltime_is_null_date(const struct icaltimetype t);
icaltimetype icaltime_merge_date_and_time(const struct icaltimetype date, const struct icaltimetype time);
icaltimetype icaltime_get_date_time_now();
bool icalrecurrencetype_compare(const struct icalrecurrencetype *a, const struct icalrecurrencetype *b);

// --- PROPERTIES --- //

typedef enum {
  DATA_PROP_COLOR,
  DATA_PROP_LIST_NAME,
  DATA_PROP_LIST_UID,
  DATA_PROP_NOTES,
  DATA_PROP_PARENT,
  DATA_PROP_RRULE,
  DATA_PROP_TEXT,
  DATA_PROP_UID,
} DataPropStr;

const char *errands_data_get_str(icalcomponent *data, DataPropStr prop);
void errands_data_set_str(icalcomponent *data, DataPropStr prop, const char *value);

typedef enum {
  DATA_PROP_PERCENT,
  DATA_PROP_PRIORITY,
} DataPropInt;

size_t errands_data_get_int(icalcomponent *data, DataPropInt prop);
void errands_data_set_int(icalcomponent *data, DataPropInt prop, size_t value);

typedef enum {
  DATA_PROP_DELETED,
  DATA_PROP_EXPANDED,
  DATA_PROP_NOTIFIED,
  DATA_PROP_TOOLBAR_SHOWN,
  DATA_PROP_TRASH,
  DATA_PROP_SYNCED,
} DataPropBool;

bool errands_data_get_bool(icalcomponent *data, DataPropBool prop);
void errands_data_set_bool(icalcomponent *data, DataPropBool prop, bool value);

typedef enum {
  DATA_PROP_ATTACHMENTS,
  DATA_PROP_TAGS,
} DataPropStrv;

GStrv errands_data_get_strv(icalcomponent *data, DataPropStrv prop);
void errands_data_set_strv(icalcomponent *data, DataPropStrv prop, GStrv value);

typedef enum {
  DATA_PROP_CHANGED_TIME,
  DATA_PROP_COMPLETED_TIME,
  DATA_PROP_CREATED_TIME,
  DATA_PROP_DUE_TIME,
  DATA_PROP_END_TIME,
  DATA_PROP_START_TIME,
} DataPropTime;

icaltimetype errands_data_get_time(icalcomponent *data, DataPropTime prop);
void errands_data_set_time(icalcomponent *data, DataPropTime prop, icaltimetype value);

void errands_data_add_tag(icalcomponent *data, DataPropStrv prop, const char *tag);
void errands_data_remove_tag(icalcomponent *data, DataPropStrv prop, const char *tag);
