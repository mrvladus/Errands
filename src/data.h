#pragma once

#include "vendor/toolbox.h"

#include <glib.h>
#include <libical/ical.h>

AUTOPTR_DEFINE(icalcomponent, icalcomponent_free)

extern GPtrArray *errands_data_lists;

// ------ ERRANDS DATA ------ //

typedef struct ErrandsData ErrandsData;
struct ErrandsData {
  icalcomponent *ical; // iCal component
  GPtrArray *children; // List of children
  // Type of data
  enum {
    ERRANDS_DATA_TYPE_LIST,
    ERRANDS_DATA_TYPE_TASK,
  } type;
  // Properties for each type of data
  union {
    // Properties for list data
    struct {
      char *uid; // List UID
    } list;
    // Properties for task data
    struct {
      ErrandsData *parent; // Parent task data
      ErrandsData *list;   // List data
    } task;
  } as;
};

#define ERRANDS_DATA_ICAL_WRAPPER(icalcomponent_ptr) ((ErrandsData){.ical = icalcomponent_ptr})

// Initialize user data
void errands_data_init(void);
// Cleanup user data
void errands_data_cleanup(void);
// Get all tasks as flat list
void errands_data_get_flat_list(GPtrArray *tasks);
// Sort all tasks
void errands_data_sort(void);

// --- LIST DATA --- //

ErrandsData *errands_list_data_new(icalcomponent *ical, const char *uid);
// Load ErrandsData from iCal component
ErrandsData *errands_list_data_load_from_ical(icalcomponent *ical, const char *uid);
ErrandsData *errands_list_data_create(const char *uid, const char *name, const char *color, bool deleted, bool synced);
void errands_list_data_sort(ErrandsData *data);
// Get all tasks as flat list
void errands_list_data_get_flat_list(ErrandsData *data, GPtrArray *tasks);
GPtrArray *errands_list_data_get_all_tasks_as_icalcomponents(ErrandsData *data);
void errands_list_data_print(ErrandsData *data);

void errands_list_data_save(ErrandsData *data);

// --- TASK DATA --- //

// Create new task and add it to parent or list children.
ErrandsData *errands_task_data_new(icalcomponent *ical, ErrandsData *parent, ErrandsData *list);
ErrandsData *errands_task_data_create_task(ErrandsData *list, ErrandsData *parent, const char *text);
void errands_task_data_sort_sub_tasks(ErrandsData *data);
size_t errands_task_data_get_indent_level(ErrandsData *data);
void errands_task_data_get_flat_list(ErrandsData *parent, GPtrArray *array);
void errands_task_data_print(ErrandsData *data);
bool errands_task_data_is_due(ErrandsData *data);
bool errands_task_data_is_completed(ErrandsData *data);
// Move task and sub-tasks recursively to another list.
// Returns the moved task.
ErrandsData *errands_task_data_move_to_list(ErrandsData *data, ErrandsData *list, ErrandsData *parent);

// --- PROPERTIES --- //

// Property type
typedef enum {
  // Bool
  PROP_CANCELLED,
  PROP_DELETED,
  PROP_EXPANDED,
  PROP_NOTIFIED,
  PROP_PINNED,
  PROP_SYNCED,
  // Integer
  PROP_PERCENT,
  PROP_PRIORITY,
  // String
  PROP_COLOR,
  PROP_LIST_NAME,
  PROP_NOTES,
  PROP_PARENT,
  PROP_RRULE,
  PROP_TEXT,
  PROP_UID,
  // Strv
  PROP_ATTACHMENTS,
  PROP_TAGS,
  // Time
  PROP_CHANGED_TIME,
  PROP_COMPLETED_TIME,
  PROP_CREATED_TIME,
  PROP_DUE_TIME,
  PROP_END_TIME,
  PROP_START_TIME,
} ErrandsProp;

// Property return value
typedef union {
  bool b;
  int i;
  const char *s;
  GStrv sv;
  icaltimetype t;
} ErrandsPropRes;

ErrandsPropRes errands_data_get_prop(const ErrandsData *data, ErrandsProp prop);
void errands_data_set_prop(ErrandsData *data, ErrandsProp prop, void *value);
void errands_data_add_tag(ErrandsData *data, const char *tag);
void errands_data_remove_tag(ErrandsData *data, const char *tag);

void errands_data_free(ErrandsData *data);
AUTOPTR_DEFINE(ErrandsData, errands_data_free);

// --- SORT AND FILTER FUNCTIONS --- //

gint errands_data_sort_func(gconstpointer a, gconstpointer b);

// --- ICAL UTILS --- //

bool icaltime_is_null_date(const struct icaltimetype t);
icaltimetype icaltime_merge_date_and_time(const struct icaltimetype date, const struct icaltimetype time);
icaltimetype icaltime_get_date_time_now(void);
bool icalrecurrencetype_compare(const struct icalrecurrencetype *a, const struct icalrecurrencetype *b);
