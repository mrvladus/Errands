#pragma once

#include "data.h"
#include "gio/gio.h"
#include "task-item.h"

#include <gtk/gtk.h>
#include <libical/ical.h>

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
  GtkWidget *priority_box;
  GtkWidget *priority_label;
  GtkWidget *tags_box;
  GtkWidget *dtstart_btn;
  GtkWidget *dtstart_btn_content;
  GtkWidget *dtend_btn;
  GtkWidget *dtend_btn_content;
  GtkWidget *rrule_btn;
  GtkWidget *rrule_btn_content;
  GtkWidget *notes_btn;
  GtkWidget *attachments_btn;
  GtkWidget *attachments_btn_content;
  GtkWidget *sub_entry;
  GtkDropControllerMotion *drop_motion_ctrl;

  // GObject Properties
  ErrandsTaskItem *item;
  TaskData *data;
  const char *color;
  const char *notes;
  icaltimetype dtstart;
  icaltimetype dtend;
  gint priority;
  GStrv tags;
  GStrv attachments;
  struct icalrecurrencetype *rrule;
};

ErrandsTask *errands_task_new();
void errands_task_update_toolbar(ErrandsTask *task);
