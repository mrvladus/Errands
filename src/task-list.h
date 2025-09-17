#pragma once

#include "data/data.h"
#include "gtk/gtk.h"
#include "task.h"

#include <adwaita.h>

// --- TASK LIST ATTACHMENTS DIALOG --- //

#define ERRANDS_TYPE_TASK_LIST_ATTACHMENTS_DIALOG (errands_task_list_attachments_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListAttachmentsDialog, errands_task_list_attachments_dialog, ERRANDS,
                     TASK_LIST_ATTACHMENTS_DIALOG, AdwDialog)

ErrandsTaskListAttachmentsDialog *errands_task_list_attachments_dialog_new();
ErrandsTask *errands_task_list_attachments_dialog_get_task(ErrandsTaskListAttachmentsDialog *self);
void errands_task_list_attachments_dialog_update_ui(ErrandsTaskListAttachmentsDialog *self);
void errands_task_list_attachments_dialog_show(ErrandsTask *task);

// --- TASK LIST ATTACHMENTS DIALOG ATTACHMENT --- //

#define ERRANDS_TYPE_TASK_LIST_ATTACHMENTS_DIALOG_ATTACHMENT                                                           \
  (errands_task_list_attachments_dialog_attachment_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListAttachmentsDialogAttachment, errands_task_list_attachments_dialog_attachment,
                     ERRANDS, TASK_LIST_ATTACHMENTS_DIALOG_ATTACHMENT, AdwActionRow)

ErrandsTaskListAttachmentsDialogAttachment *errands_task_list_attachments_dialog_attachment_new(const char *path);

// --- TASK LIST COLOR DIALOG --- //

#define ERRANDS_TYPE_TASK_LIST_COLOR_DIALOG (errands_task_list_color_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListColorDialog, errands_task_list_color_dialog, ERRANDS, TASK_LIST_COLOR_DIALOG,
                     AdwDialog)

ErrandsTaskListColorDialog *errands_task_list_color_dialog_new();
void errands_task_list_color_dialog_show(ErrandsTask *task);

// --- TASK LIST DATE DIALOG --- //

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG (errands_task_list_date_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialog, errands_task_list_date_dialog, ERRANDS, TASK_LIST_DATE_DIALOG,
                     AdwDialog)

ErrandsTaskListDateDialog *errands_task_list_date_dialog_new();
void errands_task_list_date_dialog_show(ErrandsTask *task);

// --- TASK LIST DATE DIALOG DATE CHOOSER --- //

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER (errands_task_list_date_dialog_date_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogDateChooser, errands_task_list_date_dialog_date_chooser, ERRANDS,
                     TASK_LIST_DATE_DIALOG_DATE_CHOOSER, AdwActionRow)

ErrandsTaskListDateDialogDateChooser *errands_task_list_date_dialog_date_chooser_new();
icaltimetype errands_task_list_date_dialog_date_chooser_get_date(ErrandsTaskListDateDialogDateChooser *self);
void errands_task_list_date_dialog_date_chooser_set_date(ErrandsTaskListDateDialogDateChooser *self,
                                                         const icaltimetype date);
void errands_task_list_date_dialog_date_chooser_reset(ErrandsTaskListDateDialogDateChooser *self);

// --- TASK LIST DATE DIALOG RRULE ROW --- //

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW (errands_task_list_date_dialog_rrule_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogRruleRow, errands_task_list_date_dialog_rrule_row, ERRANDS,
                     TASK_LIST_DATE_DIALOG_RRULE_ROW, AdwExpanderRow)

ErrandsTaskListDateDialogRruleRow *errands_task_list_date_dialog_rrule_row_new();
struct icalrecurrencetype errands_task_list_date_dialog_rrule_row_get_rrule(ErrandsTaskListDateDialogRruleRow *self);
void errands_task_list_date_dialog_rrule_row_set_rrule(ErrandsTaskListDateDialogRruleRow *self,
                                                       const struct icalrecurrencetype rrule);
void errands_task_list_date_dialog_rrule_row_reset(ErrandsTaskListDateDialogRruleRow *self);

// --- TASK LIST DATE DIALOG TIME CHOOSER --- //

#define ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_TIME_CHOOSER (errands_task_list_date_dialog_time_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListDateDialogTimeChooser, errands_task_list_date_dialog_time_chooser, ERRANDS,
                     DATE_DIALOG_TIME_CHOOSER, AdwActionRow)

ErrandsTaskListDateDialogTimeChooser *errands_task_list_date_dialog_time_chooser_new();
icaltimetype errands_task_list_date_dialog_time_chooser_get_time(ErrandsTaskListDateDialogTimeChooser *self);
void errands_task_list_date_dialog_time_chooser_set_time(ErrandsTaskListDateDialogTimeChooser *self,
                                                         const icaltimetype time);
void errands_task_list_date_dialog_time_chooser_reset(ErrandsTaskListDateDialogTimeChooser *self);

// --- TASK LIST NOTES DIALOG --- //

#define ERRANDS_TYPE_TASK_LIST_NOTES_DIALOG (errands_task_list_notes_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListNotesDialog, errands_task_list_notes_dialog, ERRANDS, TASK_LIST_NOTES_DIALOG,
                     AdwDialog)

ErrandsTaskListNotesDialog *errands_task_list_notes_dialog_new();
void errands_task_list_notes_dialog_show(ErrandsTask *task);

// --- TASK LIST PRIORITY DIALOG --- //

#define ERRANDS_TYPE_TASK_LIST_PRIORITY_DIALOG (errands_task_list_priority_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListPriorityDialog, errands_task_list_priority_dialog, ERRANDS,
                     TASK_LIST_PRIORITY_DIALOG, AdwDialog)

ErrandsTaskListPriorityDialog *errands_task_list_priority_dialog_new();
void errands_task_list_priority_dialog_show(ErrandsTask *task);

// --- TASK LIST TAGS DIALOG --- //

#define ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG (errands_task_list_tags_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListTagsDialog, errands_task_list_tags_dialog, ERRANDS, TASK_LIST_TAGS_DIALOG,
                     AdwDialog)

ErrandsTaskListTagsDialog *errands_task_list_tags_dialog_new();
ErrandsTask *errands_task_list_tags_dialog_get_task(ErrandsTaskListTagsDialog *self);
void errands_task_list_tags_dialog_update_ui(ErrandsTaskListTagsDialog *self);
void errands_task_list_tags_dialog_show(ErrandsTask *task);

// --- TASK LIST TAGS DIALOG TAG --- //

#define ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG_TAG (errands_task_list_tags_dialog_tag_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListTagsDialogTag, errands_task_list_tags_dialog_tag, ERRANDS,
                     TASK_LIST_TAGS_DIALOG_TAG, AdwActionRow)

ErrandsTaskListTagsDialogTag *errands_task_list_tags_dialog_tag_new(const char *tag, ErrandsTask *task);

// --- TASK LIST SORT DIALOG --- //

#define ERRANDS_TYPE_TASK_LIST_SORT_DIALOG (errands_task_list_sort_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListSortDialog, errands_task_list_sort_dialog, ERRANDS, TASK_LIST_SORT_DIALOG,
                     AdwDialog)

ErrandsTaskListSortDialog *errands_task_list_sort_dialog_new();
void errands_task_list_sort_dialog_show();

// --- TASK LIST --- //

typedef enum {
  ERRANDS_TASK_LIST_PAGE_DEFAULT,
  ERRANDS_TASK_LIST_PAGE_TRASH,
  ERRANDS_TASK_LIST_PAGE_TODAY,
} ErrandsTaskListPage;

#define ERRANDS_TYPE_TASK_LIST (errands_task_list_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskList, errands_task_list, ERRANDS, TASK_LIST, AdwBin)

struct _ErrandsTaskList {
  AdwBin parent_instance;

  GtkWidget *title;
  GtkWidget *task_list;
  GtkWidget *search_btn;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  const char *search_query;
  GtkWidget *entry_rev;

  GListStore *tasks_model;
  GtkTreeListModel *tree_model;
  GtkTreeListRowSorter *sorter;
  GtkCustomFilter *completed_filter;
  GtkCustomFilter *search_filter;
  GtkFilterListModel *search_filter_model;
  GtkCustomFilter *toplevel_filter;

  ErrandsTaskListAttachmentsDialog *attachments_dialog;
  ErrandsTaskListColorDialog *color_dialog;
  ErrandsTaskListDateDialog *date_dialog;
  ErrandsTaskListNotesDialog *notes_dialog;
  ErrandsTaskListPriorityDialog *priority_dialog;
  ErrandsTaskListSortDialog *sort_dialog;
  ErrandsTaskListTagsDialog *tags_dialog;

  ListData *data;
  ErrandsTaskListPage page;
};

ErrandsTaskList *errands_task_list_new();
void errands_task_list_load_tasks(ErrandsTaskList *self);
void errands_task_list_update_title();
