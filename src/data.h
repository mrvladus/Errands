#pragma once

#include "vendor/toolbox.h"

#include <glib.h>

#include <libical/ical.h>

AUTOPTR_DEFINE(icalcomponent, icalcomponent_free)

extern GPtrArray *errands_data_lists;

typedef struct TaskData TaskData;
typedef struct ListData ListData;

struct TaskData {
  icalcomponent *ical;
  GPtrArray *children;
  TaskData *parent;
  ListData *list;
};

#define TASK_DATA(ptr) ((TaskData *)ptr)

struct ListData {
  icalcomponent *ical;
  GPtrArray *children;
  char *uid;
};

// Initialize user data
void errands_data_init(void);
// Cleanup user data
void errands_data_cleanup(void);
// Get all tasks as flat list
void errands_data_get_flat_list(GPtrArray *tasks);
// Sort all tasks
void errands_data_sort(void);
ListData *errands_data_find_list_data_by_uid(const char *uid);

// --- LIST DATA --- //

ListData *errands_list_data_new(icalcomponent *ical, const char *uid);
// Load TaskData from iCal component
ListData *errands_list_data_load_from_ical(icalcomponent *ical, const char *uid, const char *name, const char *color);
ListData *errands_list_data_create(const char *uid, const char *name, const char *description, const char *color,
                                   bool deleted, bool synced);
void errands_list_data_sort(ListData *data);
// Get all tasks as flat list
void errands_list_data_get_flat_list(ListData *data, GPtrArray *tasks);
GPtrArray *errands_list_data_get_all_tasks_as_icalcomponents(ListData *data);
void errands_list_data_print(ListData *data);
void errands_list_data_save(ListData *data);
void errands_list_data_free(ListData *data);
AUTOPTR_DEFINE(ListData, errands_list_data_free);

// --- TASK DATA --- //

// Create new task and add it to parent or list children.
TaskData *errands_task_data_new(icalcomponent *ical, TaskData *parent, ListData *list);
TaskData *errands_task_data_create_task(ListData *list, TaskData *parent, const char *text);
void errands_task_data_sort_sub_tasks(TaskData *data);
size_t errands_task_data_get_indent_level(TaskData *data);
void errands_task_data_get_flat_list(TaskData *parent, GPtrArray *array);
void errands_task_data_print(TaskData *data);
// Move task and sub-tasks recursively to another list.
// Returns the moved task.
TaskData *errands_task_data_move_to_list(TaskData *data, ListData *list, TaskData *parent);
TaskData *errands_task_data_find_by_uid(ListData *list, const char *uid);
void errands_task_data_free(TaskData *data);
AUTOPTR_DEFINE(TaskData, errands_task_data_free);

// ---------- PROPERTIES ---------- //

// --- BOOL --- //

bool errands_data_get_cancelled(icalcomponent *ical);
bool errands_data_get_deleted(icalcomponent *ical);
bool errands_data_get_expanded(icalcomponent *ical);
bool errands_data_get_notified(icalcomponent *ical);
bool errands_data_get_pinned(icalcomponent *ical);
bool errands_data_get_synced(icalcomponent *ical);

bool errands_data_is_completed(icalcomponent *ical);
bool errands_data_is_due(icalcomponent *ical);

void errands_data_set_cancelled(icalcomponent *ical, bool value);
void errands_data_set_deleted(icalcomponent *ical, bool value);
void errands_data_set_expanded(icalcomponent *ical, bool value);
void errands_data_set_notified(icalcomponent *ical, bool value);
void errands_data_set_pinned(icalcomponent *ical, bool value);
void errands_data_set_synced(icalcomponent *ical, bool value);

// --- INT --- //

int errands_data_get_percent(icalcomponent *ical);
int errands_data_get_priority(icalcomponent *ical);

void errands_data_set_percent(icalcomponent *ical, int value);
void errands_data_set_priority(icalcomponent *ical, int value);

// --- STRING --- //

const char *errands_data_get_color(icalcomponent *ical, bool list);
const char *errands_data_get_list_name(icalcomponent *ical);
const char *errands_data_get_list_description(icalcomponent *ical);
const char *errands_data_get_notes(icalcomponent *ical);
const char *errands_data_get_parent(icalcomponent *ical);
const char *errands_data_get_text(icalcomponent *ical);
const char *errands_data_get_uid(icalcomponent *ical);

void errands_data_set_color(icalcomponent *ical, const char *value, bool list);
void errands_data_set_list_name(icalcomponent *ical, const char *value);
void errands_data_set_list_description(icalcomponent *ical, const char *value);
void errands_data_set_notes(icalcomponent *ical, const char *value);
void errands_data_set_parent(icalcomponent *ical, const char *value);
void errands_data_set_text(icalcomponent *ical, const char *value);
void errands_data_set_uid(icalcomponent *ical, const char *value);

// --- RRULE --- //

struct icalrecurrencetype errands_data_get_rrule(icalcomponent *ical);
void errands_data_set_rrule(icalcomponent *ical, struct icalrecurrencetype value);

// --- STRV --- //

GStrv errands_data_get_attachments(icalcomponent *ical);
GStrv errands_data_get_tags(icalcomponent *ical);

void errands_data_set_attachments(icalcomponent *ical, GStrv value);
void errands_data_set_tags(icalcomponent *ical, GStrv value);

void errands_data_add_tag(icalcomponent *ical, const char *tag);
void errands_data_remove_tag(icalcomponent *ical, const char *tag);

// --- TIME --- //

icaltimetype errands_data_get_changed(icalcomponent *ical);
icaltimetype errands_data_get_completed(icalcomponent *ical);
icaltimetype errands_data_get_created(icalcomponent *ical);
icaltimetype errands_data_get_due(icalcomponent *ical);
icaltimetype errands_data_get_end(icalcomponent *ical);
icaltimetype errands_data_get_start(icalcomponent *ical);

void errands_data_set_changed(icalcomponent *ical, icaltimetype value);
void errands_data_set_completed(icalcomponent *ical, icaltimetype value);
void errands_data_set_created(icalcomponent *ical, icaltimetype value);
void errands_data_set_due(icalcomponent *ical, icaltimetype value);
void errands_data_set_end(icalcomponent *ical, icaltimetype value);
void errands_data_set_start(icalcomponent *ical, icaltimetype value);

// ---------- SORT AND FILTER FUNCTIONS ---------- //

gint errands_data_sort_func(gconstpointer a, gconstpointer b);

// ---------- ICAL UTILS ---------- //

bool icaltime_is_null_date(const struct icaltimetype t);
icaltimetype icaltime_merge_date_and_time(const struct icaltimetype date, const struct icaltimetype time);
icaltimetype icaltime_get_date_time_now(void);
bool icalrecurrencetype_compare(const struct icalrecurrencetype *a, const struct icalrecurrencetype *b);
