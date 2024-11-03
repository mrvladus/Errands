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
  uint8_t percent_complete;
  uint8_t priority;
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
  char *color;
  bool deleted;
  char *name;
  bool show_completed;
  bool synced;
  char *uid;
} TaskListData;

void errands_data_load();
void errands_data_write();
TaskListData *errands_data_add_list(const char *name);
void errands_data_free_list(TaskListData *data);
void errands_data_delete_list(TaskListData *data);
// Allocated string in ICAL format for task list
char *errands_data_task_list_as_ical(TaskListData *data);
TaskData *errands_data_add_task(char *text, char *list_uid, char *parent_uid);
void errands_data_free_task(TaskData *data);
void errands_data_delete_task(const char *list_uid, const char *uid);
TaskData *errands_data_get_task(char *uid);
// Allocated string in ICAL format for task
char *errands_data_task_as_ical(TaskData *data);
// Get task list as formatted for printing string. Return value must be freed.
GString *errands_data_print_list(char *list_uid);

// --- TAGS --- //

void errands_data_tag_add(char *tag);
void errands_data_tags_free();
void errands_data_tag_remove(char *tag);
