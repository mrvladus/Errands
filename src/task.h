#pragma once

#include "data/data.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK (errands_task_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTask, errands_task, ERRANDS, TASK, GtkBox)

struct _ErrandsTask {
  GtkBox parent_instance;

  GtkWidget *clamp;

  GtkWidget *title;
  GtkWidget *edit_title;
  GtkWidget *complete_btn;
  gulong complete_btn_signal_id;
  GtkWidget *toolbar_btn;
  gulong toolbar_btn_signal_id;
  GtkWidget *tags_revealer;
  GtkWidget *progress_revealer;
  GtkWidget *progress_bar;
  GtkWidget *tags_box;
  // Toolbar
  GtkWidget *toolbar_revealer;
  GtkWidget *date_btn;
  GtkWidget *notes_btn;
  GtkWidget *priority_btn;
  GtkWidget *tags_btn;
  GtkWidget *attachments_btn;
  GtkWidget *color_btn;
  // Sub-tasks
  GtkCustomFilter *sub_tasks_filter;
  GtkFilterListModel *sub_tasks_filter_model;
  GtkWidget *sub_tasks_revealer;
  GtkWidget *sub_entry;
  GtkWidget *task_list;

  TaskData *data;
};

ErrandsTask *errands_task_new();
void errands_task_set_data(ErrandsTask *self, TaskData *data);
void errands_task_update_accent_color(ErrandsTask *task);
void errands_task_update_progress(ErrandsTask *task);
void errands_task_update_tags(ErrandsTask *task);
void errands_task_update_toolbar(ErrandsTask *task);
GPtrArray *errands_task_get_parents(ErrandsTask *task);
GPtrArray *errands_task_get_sub_tasks(ErrandsTask *task);
const char *errands_task_as_str(ErrandsTask *task);
