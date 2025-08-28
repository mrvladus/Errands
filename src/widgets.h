// Header containing all widgets declarations and public methods
#pragma once

#include "data/data.h"
#include "gio/gio.h"

#include <adwaita.h>

// --- TASK --- //

#define ERRANDS_TYPE_TASK (errands_task_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTask, errands_task, ERRANDS, TASK, GtkBox)

struct _ErrandsTask {
  GtkBox parent_instance;

  GtkWidget *complete_btn;
  GtkWidget *title;
  GtkWidget *edit_title;
  GtkWidget *toolbar_btn;
  GtkWidget *tags_revealer;
  GtkWidget *tags_box;
  GtkWidget *progress_revealer;
  GtkWidget *progress_bar;
  GtkWidget *toolbar_revealer;
  GtkWidget *date_btn;
  GtkWidget *date_btn_content;
  GtkWidget *notes_btn;
  GtkWidget *priority_btn;
  GtkWidget *tags_btn;
  GtkWidget *attachments_btn;
  GtkWidget *color_btn;
  GtkWidget *sub_entry;

  gulong complete_btn_signal_id;
  gulong toolbar_btn_signal_id;

  TaskData *data;
};

ErrandsTask *errands_task_new();
void errands_task_set_data(ErrandsTask *self, TaskData *data);
void errands_task_update_accent_color(ErrandsTask *task);
void errands_task_update_progress(ErrandsTask *task);
void errands_task_update_tags(ErrandsTask *task);
void errands_task_update_toolbar(ErrandsTask *task);
GPtrArray *errands_task_get_parents(ErrandsTask *task);
GPtrArray *errands_task_get_sub_tasks(ErrandsTask *task);
const char *errands_task_as_str(ErrandsTask *task);

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

#define ERRANDS_TYPE_TASK_LIST (errands_task_list_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskList, errands_task_list, ERRANDS, TASK_LIST, AdwBin)

struct _ErrandsTaskList {
  AdwBin parent_instance;

  GtkWidget *title;
  GtkWidget *task_list;
  GtkWidget *search_btn;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkWidget *entry_rev;

  GListStore *tasks_model;
  GtkTreeListModel *tree_model;
  GtkTreeListRowSorter *tree_sorter;
  GtkSortListModel *sorted_model;
  GtkCustomFilter *toplevel_tasks_filter;
  GtkFilterListModel *toplevel_tasks_filter_model;
  GtkCustomSorter *sorter;

  ErrandsTaskListAttachmentsDialog *attachments_dialog;
  ErrandsTaskListColorDialog *color_dialog;
  ErrandsTaskListDateDialog *date_dialog;
  ErrandsTaskListNotesDialog *notes_dialog;
  ErrandsTaskListPriorityDialog *priority_dialog;
  ErrandsTaskListSortDialog *sort_dialog;
  ErrandsTaskListTagsDialog *tags_dialog;

  ListData *data;
};

ErrandsTaskList *errands_task_list_new();
void errands_task_list_load_tasks(ErrandsTaskList *self);
void errands_task_list_add(TaskData *td);
void errands_task_list_update_title();
void setup_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
void bind_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);

// --- SIDEBAR TASK LIST ROW --- //

#define ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW (errands_sidebar_task_list_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row, ERRANDS, SIDEBAR_TASK_LIST_ROW,
                     GtkListBoxRow)

struct _ErrandsSidebarTaskListRow {
  GtkListBoxRow parent_instance;
  GtkWidget *color_btn;
  GtkWidget *counter;
  GtkWidget *label;
  GtkEventController *hover_ctrl;

  ListData *data;
};

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_new(ListData *data);
void errands_sidebar_task_list_row_update_counter(ErrandsSidebarTaskListRow *row);
void errands_sidebar_task_list_row_update_title(ErrandsSidebarTaskListRow *row);
ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(const char *uid);
void on_errands_sidebar_task_list_row_activate(GtkListBox *box, ErrandsSidebarTaskListRow *row, gpointer user_data);

// --- SIDEBAR ALL ROW --- //

#define ERRANDS_TYPE_SIDEBAR_ALL_ROW (errands_sidebar_all_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarAllRow, errands_sidebar_all_row, ERRANDS, SIDEBAR_ALL_ROW, GtkListBoxRow)

ErrandsSidebarAllRow *errands_sidebar_all_row_new();
void errands_sidebar_all_row_update_counter(ErrandsSidebarAllRow *row);

// --- SIDEBAR DELETE LIST DIALOG --- //

#define ERRANDS_TYPE_SIDEBAR_DELETE_LIST_DIALOG (errands_sidebar_delete_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarDeleteListDialog, errands_sidebar_delete_list_dialog, ERRANDS,
                     SIDEBAR_DELETE_LIST_DIALOG, AdwAlertDialog)

ErrandsSidebarDeleteListDialog *errands_sidebar_delete_list_dialog_new();
void errands_sidebar_delete_list_dialog_show(ErrandsSidebarTaskListRow *row);

// --- SIDEBAR NEW LIST DIALOG --- //

#define ERRANDS_TYPE_SIDEBAR_NEW_LIST_DIALOG (errands_sidebar_new_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarNewListDialog, errands_sidebar_new_list_dialog, ERRANDS, SIDEBAR_NEW_LIST_DIALOG,
                     AdwAlertDialog)

ErrandsSidebarNewListDialog *errands_sidebar_new_list_dialog_new();
void errands_sidebar_new_list_dialog_show();

// --- SIDEBAR RENAME LIST DIALOG --- //

#define ERRANDS_TYPE_SIDEBAR_RENAME_LIST_DIALOG (errands_sidebar_rename_list_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebarRenameListDialog, errands_sidebar_rename_list_dialog, ERRANDS,
                     SIDEBAR_RENAME_LIST_DIALOG, AdwAlertDialog)

ErrandsSidebarRenameListDialog *errands_sidebar_rename_list_dialog_new();
void errands_sidebar_rename_list_dialog_show(ErrandsSidebarTaskListRow *row);

// --- SIDEBAR --- //

#define ERRANDS_TYPE_SIDEBAR (errands_sidebar_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSidebar, errands_sidebar, ERRANDS, SIDEBAR, AdwBin)

struct _ErrandsSidebar {
  AdwBin parent_instance;
  GtkWidget *add_btn;
  GtkWidget *filters_box;
  ErrandsSidebarAllRow *all_row;
  GtkWidget *task_lists_box;
  ErrandsSidebarTaskListRow *current_task_list_row;
  ErrandsSidebarDeleteListDialog *delete_list_dialog;
  ErrandsSidebarNewListDialog *new_list_dialog;
  ErrandsSidebarRenameListDialog *rename_list_dialog;
};

ErrandsSidebar *errands_sidebar_new();
void errands_sidebar_load_lists(ErrandsSidebar *self);
ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(ErrandsSidebar *sb, ListData *data);
void errands_sidebar_select_last_opened_page();

// --- WINDOW --- //

#define ERRANDS_TYPE_WINDOW (errands_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsWindow, errands_window, ERRANDS, WINDOW, AdwApplicationWindow)

struct _ErrandsWindow {
  AdwApplicationWindow parent_instance;
  GtkWidget *no_lists_page;
  ErrandsTaskList *task_list;
  GtkWidget *toast_overlay;
  ErrandsSidebar *sidebar;
  GtkWidget *split_view;
  GtkWidget *stack;
};

ErrandsWindow *errands_window_new(GtkApplication *app);
void errands_window_update(ErrandsWindow *win);
void errands_window_add_toast(ErrandsWindow *win, const char *msg);
