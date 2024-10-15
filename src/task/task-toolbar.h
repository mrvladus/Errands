#pragma once

#include <gtk/gtk.h>

#define ERRANDS_TYPE_TASK_TOOLBAR (errands_task_toolbar_get_type())

G_DECLARE_FINAL_TYPE(ErrandsTaskToolbar, errands_task_toolbar, ERRANDS,
                     TASK_TOOLBAR, GtkBox)

struct _ErrandsTaskToolbar {
  GtkBox parent_instance;
  GtkWidget *date_btn;
  GtkWidget *notes_btn;
  GtkWidget *priority_btn;
  GtkWidget *tags_btn;
  GtkWidget *attachments_btn;
  GtkWidget *color_btn;
};

ErrandsTaskToolbar *errands_task_toolbar_new(void *task);
