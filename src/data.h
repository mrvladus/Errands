#pragma once

#include <glib.h>
#include <libical/ical.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef icalcomponent ListData;
typedef icalcomponent TaskData;

ListData *list_data_new(const char *uid);
void list_data_free(ListData *data);
GPtrArray *list_data_get_tasks(ListData *data);
TaskData *list_data_create_task(ListData *list, const char *text, const char *list_uid,
                                const char *parent);

const char *list_data_get_color(ListData *data);
void list_data_set_color(ListData *data, const char *color);
const char *list_data_get_name(ListData *data);
void list_data_set_name(ListData *data, const char *name);
const char *list_data_get_uid(ListData *data);
void list_data_set_uid(ListData *data, const char *uid);
bool list_data_get_deleted(ListData *data);
void list_data_set_deleted(ListData *data, bool deleted);
bool list_data_get_synced(ListData *data);
void list_data_set_synced(ListData *data, bool synced);

TaskData *task_data_new(ListData *list, const char *text, const char *parent);
void task_data_free(TaskData *data);
ListData *task_data_get_list(TaskData *data);

const char *task_data_get_changed(TaskData *data);
void task_data_set_changed(TaskData *data, const char *changed);
const char *task_data_get_created(TaskData *data);
void task_data_set_created(TaskData *data, const char *created);
const char *task_data_get_due(TaskData *data);
void task_data_set_due(TaskData *data, const char *due);
const char *task_data_get_notes(TaskData *data);
void task_data_set_notes(TaskData *data, const char *notes);
const char *task_data_get_parent(TaskData *data);
void task_data_set_parent(TaskData *data, const char *parent);
const char *task_data_get_start(TaskData *data);
void task_data_set_start(TaskData *data, const char *start);
const char *task_data_get_rrule(TaskData *data);
void task_data_set_rrule(TaskData *data, const char *rrule);
const char *task_data_get_text(TaskData *data);
void task_data_set_text(TaskData *data, const char *text);
const char *task_data_get_uid(TaskData *data);
void task_data_set_uid(TaskData *data, const char *uid);
bool task_data_get_completed(TaskData *data);
void task_data_set_completed(TaskData *data, bool completed);
const char *task_data_get_tags(TaskData *data);
void task_data_set_tags(TaskData *data, const char *tags);
uint8_t task_data_get_percent(TaskData *data);
void task_data_set_percent(TaskData *data, uint8_t percent);
uint8_t task_data_get_priority(TaskData *data);
void task_data_set_priority(TaskData *data, uint8_t priority);
const char *task_data_get_attachments(TaskData *data);
void task_data_set_attachments(TaskData *data, const char *attachments);
const char *task_data_get_color(TaskData *data);
void task_data_set_color(TaskData *data, const char *color);
const char *task_data_get_list_uid(TaskData *data);
void task_data_set_list_uid(TaskData *data, const char *list_uid);
bool task_data_get_deleted(TaskData *data);
void task_data_set_deleted(TaskData *data, bool deleted);
bool task_data_get_expanded(TaskData *data);
void task_data_set_expanded(TaskData *data, bool expanded);
bool task_data_get_notified(TaskData *data);
void task_data_set_notified(TaskData *data, bool notified);
bool task_data_get_synced(TaskData *data);
void task_data_set_synced(TaskData *data, bool synced);
bool task_data_get_toolbar_shown(TaskData *data);
void task_data_set_toolbar_shown(TaskData *data, bool toolbar_shown);
bool task_data_get_trash(TaskData *data);
void task_data_set_trash(TaskData *data, bool trash);

GPtrArray *errands_data_load_lists();
GPtrArray *errands_data_load_tasks(GPtrArray *lists);
void errands_data_write_list(ListData *data);
void errands_data_write_lists(GPtrArray *lists);
