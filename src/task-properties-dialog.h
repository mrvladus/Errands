#pragma once

#include "task-item.h"

#include <adwaita.h>
#include <libical-glib/libical-glib.h>

#define ERRANDS_TYPE_TASK_PROPERTIES_DIALOG (errands_task_properties_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskPropertiesDialog, errands_task_properties_dialog, ERRANDS, TASK_PROPERTIES_DIALOG, AdwDialog)

typedef enum {
  ERRANDS_TASK_PROPERTY_DIALOG_PAGE_DATE,
  ERRANDS_TASK_PROPERTY_DIALOG_PAGE_NOTES,
  ERRANDS_TASK_PROPERTY_DIALOG_PAGE_ATTACHMENTS,
  ERRANDS_TASK_PROPERTY_DIALOG_PAGE_TAGS,

  ERRANDS_TASK_PROPERTY_DIALOG_N_PAGES
} ErrandsTaskPropertiesDialogPage;

ErrandsTaskPropertiesDialog *errands_task_properties_dialog_new();
void errands_task_properties_dialog_show(ErrandsTaskPropertiesDialogPage page, ErrandsTaskItem *item);

// --- RRULE ROW --- //

#define ERRANDS_TYPE_RRULE_ROW (errands_rrule_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsRruleRow, errands_rrule_row, ERRANDS, TASK_RRULE_ROW, AdwExpanderRow)

ErrandsRruleRow *errands_rrule_row_new();
ICalRecurrence *errands_rrule_row_get_rrule(ErrandsRruleRow *self);
void errands_rrule_row_set_rrule(ErrandsRruleRow *self, ICalRecurrence *rrule);
void errands_rrule_row_reset(ErrandsRruleRow *self);
