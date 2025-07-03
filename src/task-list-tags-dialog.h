#pragma once

#include "task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG (errands_task_list_tags_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListTagsDialog, errands_task_list_tags_dialog, ERRANDS, TASK_LIST_TAGS_DIALOG,
                     AdwDialog)

struct _ErrandsTaskListTagsDialog {
  AdwDialog parent_instance;
  GtkWidget *entry;
  GtkWidget *tags_box;
  GtkWidget *placeholder;

  ErrandsTask *current_task;
};

ErrandsTaskListTagsDialog *errands_task_list_tags_dialog_new();
void errands_task_list_tags_dialog_update_ui(ErrandsTaskListTagsDialog *self);
void errands_task_list_tags_dialog_show(ErrandsTask *task);
