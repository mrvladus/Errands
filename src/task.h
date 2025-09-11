#pragma once

#include "data/data.h"

#include <gtk/gtk.h>

#define ERRANDS_TYPE_TASK (errands_task_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTask, errands_task, ERRANDS, TASK, GtkBox)

struct _ErrandsTask {
  GtkBox parent_instance;

  GtkWidget *complete_btn;
  GtkWidget *title;
  GtkWidget *edit_title;
  GtkWidget *toolbar_btn;
  GtkWidget *tags_revealer;
  GtkWidget *tags_box;
  GtkWidget *progress_revealer;
  GtkWidget *progress_bar;
  GtkWidget *toolbar_revealer;
  GtkWidget *date_btn;
  GtkWidget *date_btn_content;
  GtkWidget *notes_btn;
  GtkWidget *priority_btn;
  GtkWidget *tags_btn;
  GtkWidget *attachments_btn;
  GtkWidget *color_btn;
  GtkWidget *sub_entry;

  gulong complete_btn_signal_id;
  gulong toolbar_btn_signal_id;

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
