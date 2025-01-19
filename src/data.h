#pragma once

#include <glib.h>
#include <libical/ical.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DECLARE_LIST_PROPERTY(type, property)                                                      \
  type list_data_get_##property(ListData *data);                                                   \
  void list_data_set_##property(ListData *data, type property);

#define DECLARE_TASK_PROPERTY(type, property)                                                      \
  type task_data_get_##property(TaskData *data);                                                   \
  void task_data_set_##property(TaskData *data, type property);

typedef icalcomponent ListData;
typedef icalcomponent TaskData;

ListData *list_data_new(const char *uid);
void list_data_free(ListData *data);
GPtrArray *list_data_get_tasks(ListData *data);
TaskData *list_data_create_task(ListData *list, const char *text, const char *list_uid,
                                const char *parent);

DECLARE_LIST_PROPERTY(const char *, color);
DECLARE_LIST_PROPERTY(const char *, name);
DECLARE_LIST_PROPERTY(const char *, uid);
DECLARE_LIST_PROPERTY(bool, deleted);
DECLARE_LIST_PROPERTY(bool, synced);

TaskData *task_data_new(ListData *list, const char *text, const char *parent);
void task_data_free(TaskData *data);
ListData *task_data_get_list(TaskData *data);

DECLARE_TASK_PROPERTY(const char *, changed);
DECLARE_TASK_PROPERTY(const char *, created);
DECLARE_TASK_PROPERTY(const char *, due);
DECLARE_TASK_PROPERTY(const char *, notes);
DECLARE_TASK_PROPERTY(const char *, parent);
DECLARE_TASK_PROPERTY(const char *, start);
DECLARE_TASK_PROPERTY(const char *, rrule);
DECLARE_TASK_PROPERTY(const char *, text);
DECLARE_TASK_PROPERTY(const char *, uid);
DECLARE_TASK_PROPERTY(const char *, tags);
DECLARE_TASK_PROPERTY(const char *, attachments);
DECLARE_TASK_PROPERTY(const char *, color);
DECLARE_TASK_PROPERTY(const char *, list_uid);
DECLARE_TASK_PROPERTY(uint8_t, percent);
DECLARE_TASK_PROPERTY(uint8_t, priority);
DECLARE_TASK_PROPERTY(bool, completed);
DECLARE_TASK_PROPERTY(bool, completed);
DECLARE_TASK_PROPERTY(bool, deleted);
DECLARE_TASK_PROPERTY(bool, expanded);
DECLARE_TASK_PROPERTY(bool, notified);
DECLARE_TASK_PROPERTY(bool, synced);
DECLARE_TASK_PROPERTY(bool, toolbar_shown);
DECLARE_TASK_PROPERTY(bool, trash);

GPtrArray *errands_data_load_lists();
GPtrArray *errands_data_load_tasks(GPtrArray *lists);
void errands_data_write_list(ListData *data);
void errands_data_write_lists(GPtrArray *lists);
