#pragma once

#include <glib.h>
#include <libical/ical.h>

#include <stdbool.h>
#include <stdint.h>

extern GPtrArray *ldata;
extern GPtrArray *tdata;

typedef icalcomponent ListData;
typedef icalcomponent TaskData;

ListData *list_data_new(const char *uid, const char *name, const char *color, bool deleted,
                        bool synced, int position);
ListData *list_data_new_from_ical(const char *ical, const char *uid, size_t position);
void list_data_free(ListData *data);
GPtrArray *list_data_get_tasks(ListData *data);
TaskData *list_data_create_task(ListData *list, const char *text, const char *list_uid,
                                const char *parent);
gchar *list_data_print(ListData *data);

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
int list_data_get_position(ListData *data);
void list_data_set_position(ListData *data, int position);
GStrv list_data_get_tags(ListData *data);
// Get all tags from all the lists
GStrv list_data_get_all_tags();
void list_data_add_tag(ListData *data, const char *tag);
void list_data_remove_tag(ListData *data, const char *tag);
void list_data_set_tags(ListData *data, GStrv tags);

TaskData *task_data_new(ListData *list, const char *text, const char *parent);
void task_data_free(TaskData *data);
ListData *task_data_get_list(TaskData *data);
GPtrArray *task_data_get_children(TaskData *data);
void task_data_print(TaskData *data, GString *out, size_t indent);

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
// Returns NULL on error
GStrv task_data_get_tags(TaskData *data);
void task_data_add_tag(TaskData *data, const char *tag);
void task_data_remove_tag(TaskData *data, const char *tag);
// Returns NULL on error
GStrv task_data_get_attachments(TaskData *data);
void task_data_set_attachments(TaskData *data, GStrv attachments);
const char *task_data_get_color(TaskData *data);
void task_data_set_color(TaskData *data, const char *color);
const char *task_data_get_list_uid(TaskData *data);
void task_data_set_list_uid(TaskData *data, const char *list_uid);
uint8_t task_data_get_percent(TaskData *data);
void task_data_set_percent(TaskData *data, uint8_t percent);
uint8_t task_data_get_priority(TaskData *data);
void task_data_set_priority(TaskData *data, uint8_t priority);
bool task_data_get_completed(TaskData *data);
void task_data_set_completed(TaskData *data, bool completed);
bool task_data_get_completed(TaskData *data);
void task_data_set_completed(TaskData *data, bool completed);
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

void errands_data_load_lists();
void errands_data_write_list(ListData *data);
