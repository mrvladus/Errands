#ifndef ERRANDS_DATA_H
#define ERRANDS_DATA_H

#include "utils.h"

#include <glib.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  // Associated task widget
  GtkWidget *task;

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

TaskData *errands_data_add_task(char *text, char *list_uid, char *parent_uid);
TaskListData *errands_data_add_list(const char *name);

TaskData *errands_data_get_task(char *uid);

void errands_data_print(char *list_uid, char *file_path);

#endif // ERRANDS_DATA_H
