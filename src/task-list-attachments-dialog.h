#pragma once

#include "task/task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_ATTACHMENTS_DIALOG (errands_task_list_attachments_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListAttachmentsDialog, errands_task_list_attachments_dialog, ERRANDS,
                     TASK_LIST_ATTACHMENTS_DIALOG, AdwDialog)

struct _ErrandsTaskListAttachmentsDialog {
  AdwDialog parent_instance;
  GtkWidget *attachments_box;
  GtkWidget *placeholder;

  ErrandsTask *current_task;
};

ErrandsTaskListAttachmentsDialog *errands_task_list_attachments_dialog_new();
void errands_task_list_attachments_dialog_update_ui(ErrandsTaskListAttachmentsDialog *self);
void errands_task_list_attachments_dialog_show(ErrandsTask *task);
