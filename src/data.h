#pragma once

#include <gtk/gtk.h>

#include <stdbool.h>
#include <stdint.h>

// Free allocated string and allocate new_string
#define errands_data_update_str(string, new_string)                                                \
  free(string);                                                                                    \
  string = strdup(new_string);

typedef struct {
  GPtrArray *attachments;
  char *color;
  bool completed;
  char *changed_at;
  char *created_at;
  bool deleted;
  char *due_date;
  bool expanded;
  char *list_uid;
  char *notes;
  bool notified;
  char *parent;
  int percent_complete;
  int priority;
  char *rrule;
  char *start_date;
  bool synced;
  GPtrArray *tags;
  char *text;
  bool toolbar_shown;
  bool trash;
  char *uid;
} TaskData;

typedef struct {
  char color[10];
  bool deleted;
  char *name;
  bool show_completed;
  bool synced;
  char uid[37];
} TaskListData;

// Load user data from data.json
void errands_data_load();
// Write user data to data.json
void errands_data_write();
// Add new task list with name
TaskListData *errands_data_add_list(const char *name);
void errands_data_free_list(TaskListData *data);
void errands_data_delete_list(TaskListData *data);
// Allocated string in ICAL format for task list
char *errands_data_task_list_as_ical(TaskListData *data);
TaskData *errands_data_add_task(char *text, char *list_uid, char *parent_uid);
void errands_data_free_task(TaskData *data);
TaskData *errands_data_get_task(char *uid);
// Allocated string in ICAL format for task
char *errands_data_task_as_ical(TaskData *data);
GPtrArray *errands_data_tasks_from_ical(const char *ical, const char *list_uid);
// Create TaskListData from ical string
TaskListData *errands_task_list_from_ical(const char *ical);
// Get task list as formatted for printing string. Return value must be freed.
GString *errands_data_print_list(char *list_uid);

// --- TAGS --- //

void errands_data_tag_add(char *tag);
void errands_data_tags_free();
void errands_data_tag_remove(char *tag);
