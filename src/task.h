#pragma once

#include "data.h"

#include <gtk/gtk.h>

#define ERRANDS_TYPE_TASK (errands_task_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTask, errands_task, ERRANDS, TASK, GtkBox)

struct _ErrandsTask {
  GtkBox parent_instance;

  GtkWidget *complete_btn;
  GtkWidget *title;
  GtkWidget *subtitle;
  GtkWidget *edit_title;
  GtkWidget *toolbar;
  GtkWidget *props_bar;
  GtkWidget *tags_box;
  GtkWidget *date_btn;
  GtkWidget *date_btn_content;
  GtkWidget *unpin_btn;
  GtkWidget *notes_btn;
  GtkWidget *priority_btn;
  GtkWidget *attachments_btn;
  GtkLabel *attachments_count;
  GtkWidget *sub_entry;

  TaskData *data;
};

ErrandsTask *errands_task_new();
void errands_task_set_data(ErrandsTask *self, TaskData *data);
void errands_task_update_accent_color(ErrandsTask *task);
void errands_task_update_progress(ErrandsTask *task);
void errands_task_update_toolbar(ErrandsTask *task);
