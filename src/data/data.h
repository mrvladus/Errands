#pragma once

#include <glib.h>
#include <libical/ical.h>

#include <stdbool.h>
#include <stdint.h>

extern GHashTable *ldata;
extern GHashTable *tdata;

typedef icalcomponent ListData;
typedef icalcomponent TaskData;

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

typedef enum {
  DATA_PROP_PERCENT,
  DATA_PROP_PRIORITY,
} DataPropInt;

typedef enum {
  DATA_PROP_DELETED,
  DATA_PROP_EXPANDED,
  DATA_PROP_NOTIFIED,
  DATA_PROP_TOOLBAR_SHOWN,
  DATA_PROP_TRASH,
  DATA_PROP_SYNCED,
} DataPropBool;

typedef enum {
  DATA_PROP_ATTACHMENTS,
  DATA_PROP_TAGS,
} DataPropStrv;

typedef enum {
  DATA_PROP_CHANGED_TIME,
  DATA_PROP_COMPLETED_TIME,
  DATA_PROP_CREATED_TIME,
  DATA_PROP_DUE_TIME,
  DATA_PROP_END_TIME,
  DATA_PROP_START_TIME,
} DataPropTime;

void errands_data_load_lists();
void errands_data_write_list(ListData *data);

const char *errands_data_get_str(icalcomponent *data, DataPropStr prop);
size_t errands_data_get_int(icalcomponent *data, DataPropInt prop);
bool errands_data_get_bool(icalcomponent *data, DataPropBool prop);
GStrv errands_data_get_strv(icalcomponent *data, DataPropStrv prop);
icaltimetype errands_data_get_time(icalcomponent *data, DataPropTime prop);

void errands_data_set_str(icalcomponent *data, DataPropStr prop, const char *value);
void errands_data_set_int(icalcomponent *data, DataPropInt prop, size_t value);
void errands_data_set_bool(icalcomponent *data, DataPropBool prop, bool value);
void errands_data_set_strv(icalcomponent *data, DataPropStrv prop, GStrv value);
void errands_data_set_time(icalcomponent *data, DataPropTime prop, icaltimetype value);

void errands_data_add_tag(icalcomponent *data, DataPropStrv prop, const char *tag);
void errands_data_remove_tag(icalcomponent *data, DataPropStrv prop, const char *tag);

void errands_data_free(icalcomponent *data);

ListData *list_data_new(const char *uid, const char *name, const char *color, bool deleted, bool synced, int position);
ListData *list_data_new_from_ical(const char *ical, const char *uid, size_t position);
gchar *list_data_print(ListData *data);
GPtrArray *list_data_get_tasks(ListData *data);
TaskData *list_data_create_task(ListData *list, const char *text, const char *list_uid, const char *parent);

TaskData *task_data_new(ListData *list, const char *text, const char *parent);
void task_data_print(TaskData *data, GString *out, size_t indent);
ListData *task_data_get_list(TaskData *data);
GPtrArray *task_data_get_children(TaskData *data);

bool icaltime_is_null_date(const struct icaltimetype t);
icaltimetype icaltime_merge_date_and_time(const struct icaltimetype date, const struct icaltimetype time);
icaltimetype icaltime_get_date_time_now();
