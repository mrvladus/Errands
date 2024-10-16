#pragma once

#include "../data.h"
#include "task-toolbar.h"

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
  GtkWidget *edit_row;
  GtkWidget *toolbar_btn;
  GtkWidget *tags_revealer;
  GtkWidget *tags_box;
  GtkWidget *toolbar_revealer;
  ErrandsTaskToolbar *toolbar;
  GtkWidget *sub_tasks_revealer;
  GtkWidget *sub_entry;
  GtkWidget *sub_tasks;
  TaskData *data;
};

ErrandsTask *errands_task_new(TaskData *data);
void errands_task_update_accent_color(ErrandsTask *task);
void errands_task_update_tags(ErrandsTask *task);
