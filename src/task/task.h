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
  GtkWidget *progress_revealer;
  GtkWidget *progress_bar;
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
void errands_task_update_progress(ErrandsTask *task);
void errands_task_update_tags(ErrandsTask *task);
// Return list of ErrandsTask widgets from parent to great ... great parent
// Free with g_ptr_array_free(arr, true)
GPtrArray *errands_task_get_parents(ErrandsTask *task);
