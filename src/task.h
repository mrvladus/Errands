#pragma once

#include "data.h"
#include "gio/gio.h"
#include "task-item.h"

#include <gtk/gtk.h>

#define ERRANDS_TYPE_TASK (errands_task_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTask, errands_task, ERRANDS, TASK, GtkBox)

struct _ErrandsTask {
  GtkBox parent_instance;

  GSimpleActionGroup *ag;

  GtkWidget *complete_btn;
  GtkWidget *title;
  GtkWidget *edit_title;
  GtkWidget *popover_menu;
  GtkWidget *toolbar;
  GtkWidget *props_bar;
  GtkWidget *priority_box;
  GtkWidget *priority_label;
  GtkWidget *tags_box;
  GtkWidget *date_btn;
  GtkWidget *date_btn_content;
  GtkWidget *notes_btn;
  GtkWidget *attachments_btn;
  GtkLabel *attachments_count;
  GtkWidget *sub_entry;
  GtkDropControllerMotion *drop_motion_ctrl;

  // GObject Properties
  ErrandsTaskItem *item;
  TaskData *data;
  const char *color;
  gint priority;
};

ErrandsTask *errands_task_new();
void errands_task_set_data(ErrandsTask *self, TaskData *data);
void errands_task_update_toolbar(ErrandsTask *task);
