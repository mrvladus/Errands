#pragma once

#include <adwaita.h>
#include <libical/ical.h>

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW (errands_task_list_date_dialog_rrule_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogRruleRow, errands_task_list_date_dialog_rrule_row, ERRANDS,
                     TASK_LIST_DATE_DIALOG_RRULE_ROW, AdwExpanderRow)

ErrandsTaskListDateDialogRruleRow *errands_task_list_date_dialog_rrule_row_new();
struct icalrecurrencetype errands_task_list_date_dialog_rrule_row_get_rrule(ErrandsTaskListDateDialogRruleRow *self);
void errands_task_list_date_dialog_rrule_row_set_rrule(ErrandsTaskListDateDialogRruleRow *self,
                                                       const struct icalrecurrencetype rrule);
void errands_task_list_date_dialog_rrule_row_reset(ErrandsTaskListDateDialogRruleRow *self);
