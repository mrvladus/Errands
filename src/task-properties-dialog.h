#pragma once

#include "task.h"

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_PROPERTIES_DIALOG (errands_task_properties_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskPropertiesDialog, errands_task_properties_dialog, ERRANDS, TASK_PROPERTIES_DIALOG,
                     AdwDialog)

typedef enum {
  ERRANDS_TASK_PROPERTY_DIALOG_PAGE_DATE,
  ERRANDS_TASK_PROPERTY_DIALOG_PAGE_NOTES,
  ERRANDS_TASK_PROPERTY_DIALOG_PAGE_PRIORITY,
  ERRANDS_TASK_PROPERTY_DIALOG_PAGE_ATTACHMENTS,
  ERRANDS_TASK_PROPERTY_DIALOG_PAGE_TAGS,

  ERRANDS_TASK_PROPERTY_DIALOG_N_PAGES
} ErrandsTaskPropertiesDialogPage;

ErrandsTaskPropertiesDialog *errands_task_properties_dialog_new();
void errands_task_properties_dialog_show(ErrandsTaskPropertiesDialogPage page, ErrandsTask *task);
