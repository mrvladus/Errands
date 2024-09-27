#ifndef ERRANDS_TASK_H
#define ERRANDS_TASK_H

#include "data.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK (errands_task_get_type())

G_DECLARE_FINAL_TYPE(ErrandsTask, errands_task, ERRANDS, TASK, AdwBin)

struct _ErrandsTask {
  AdwBin parent_instance;
  GtkWidget *revealer;
  GtkWidget *card;
  GtkWidget *title_row;
  GtkGesture *click_ctrl;
  GtkWidget *complete_btn;
  GtkWidget *toolbar_btn;
  GtkWidget *toolbar_revealer;
  GtkWidget *sub_tasks_revealer;
  GtkWidget *sub_entry;
  GtkWidget *sub_tasks;
  TaskData *data;
};

ErrandsTask *errands_task_new(TaskData *data);

// GtkWidget *errands_task_new(TaskData *td);

#endif // ERRANDS_TASK_H
