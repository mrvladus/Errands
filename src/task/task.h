#pragma once

#include "../data/data.h"

#include <adwaita.h>

// --- DECLARE WIDGETS --- //

#define ERRANDS_TYPE_TASK (errands_task_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTask, errands_task, ERRANDS, TASK, AdwBin)
#define ERRANDS_TYPE_DATE_CHOOSER (errands_date_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsDateChooser, errands_date_chooser, ERRANDS, DATE_CHOOSER, GtkBox)
#define ERRANDS_TYPE_MONTH_CHOOSER (errands_month_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsMonthChooser, errands_month_chooser, ERRANDS, MONTH_CHOOSER, GtkListBoxRow)
#define ERRANDS_TYPE_TIME_CHOOSER (errands_time_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTimeChooser, errands_time_chooser, ERRANDS, TIME_CHOOSER, GtkBox)
#define ERRANDS_TYPE_WEEK_CHOOSER (errands_week_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsWeekChooser, errands_week_chooser, ERRANDS, WEEK_CHOOSER, GtkListBoxRow)
#define ERRANDS_TYPE_DATE_WINDOW (errands_date_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsDateWindow, errands_date_window, ERRANDS, DATE_WINDOW, AdwDialog)
#define ERRANDS_TYPE_COLOR_WINDOW (errands_color_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsColorWindow, errands_color_window, ERRANDS, COLOR_WINDOW, AdwDialog)
#define ERRANDS_TYPE_ATTACHMENTS_WINDOW (errands_attachments_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsAttachmentsWindow, errands_attachments_window, ERRANDS, ATTACHMENTS_WINDOW, AdwDialog)
#define ERRANDS_TYPE_ATTACHMENTS_WINDOW_ROW (errands_attachments_window_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsAttachmentsWindowRow, errands_attachments_window_row, ERRANDS, ATTACHMENTS_WINDOW_ROW,
                     AdwActionRow)
#define ERRANDS_TYPE_TAGS_WINDOW (errands_tags_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTagsWindow, errands_tags_window, ERRANDS, TAGS_WINDOW, AdwDialog)
#define ERRANDS_TYPE_TAGS_WINDOW_ROW (errands_tags_window_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTagsWindowRow, errands_tags_window_row, ERRANDS, TAGS_WINDOW_ROW, AdwActionRow)
#define ERRANDS_TYPE_NOTES_WINDOW (errands_notes_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsNotesWindow, errands_notes_window, ERRANDS, NOTES_WINDOW, AdwDialog)
#define ERRANDS_TYPE_PRIORITY_WINDOW (errands_priority_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsPriorityWindow, errands_priority_window, ERRANDS, PRIORITY_WINDOW, AdwDialog)

// --- TASK --- //

struct _ErrandsTask {
  AdwBin parent_instance;

  GtkWidget *clamp;

  GtkWidget *title;
  GtkWidget *edit_title;
  GtkWidget *complete_btn;
  GtkWidget *toolbar_btn;
  GtkWidget *tags_revealer;
  GtkWidget *progress_revealer;
  GtkWidget *progress_bar;
  GtkWidget *tags_box;
  // Toolbar
  GtkWidget *toolbar_revealer;
  GtkWidget *date_btn;
  GtkWidget *notes_btn;
  GtkWidget *priority_btn;
  GtkWidget *tags_btn;
  GtkWidget *attachments_btn;
  GtkWidget *color_btn;
  // Sub-tasks
  GtkWidget *sub_tasks_revealer;
  GtkWidget *sub_entry;
  GtkWidget *sub_tasks;

  TaskData *data;
};

ErrandsTask *errands_task_new();
void errands_task_set_data(ErrandsTask *task, TaskData *data);
void errands_task_update_accent_color(ErrandsTask *task);
void errands_task_update_progress(ErrandsTask *task);
void errands_task_update_tags(ErrandsTask *task);
void errands_task_update_toolbar(ErrandsTask *task);
GPtrArray *errands_task_get_parents(ErrandsTask *task);
GPtrArray *errands_task_get_sub_tasks(ErrandsTask *task);

// --- DATE WINDOW --- //

struct _ErrandsDateChooser {
  GtkBox parent_instance;
  GtkWidget *calendar;
  GtkWidget *label;
  GtkWidget *reset_btn;
};

ErrandsDateChooser *errands_date_chooser_new();
// Get string in format YYYYMMDD
const char *errands_date_chooser_get_date(ErrandsDateChooser *dc);
void errands_date_chooser_set_date(ErrandsDateChooser *dc, const char *yyyymmss);
void errands_date_chooser_reset(ErrandsDateChooser *dc);

struct _ErrandsMonthChooser {
  GtkListBoxRow parent_instance;
  GtkWidget *month_box1;
  GtkWidget *month_box2;
};
ErrandsMonthChooser *errands_month_chooser_new();
const char *errands_month_chooser_get_months(ErrandsMonthChooser *mc);
void errands_month_chooser_set_months(ErrandsMonthChooser *mc, const char *rrule);
void errands_month_chooser_reset(ErrandsMonthChooser *mc);

struct _ErrandsTimeChooser {
  GtkBox parent_instance;
  GtkWidget *hours;
  GtkWidget *minutes;
  GtkWidget *label;
  GtkWidget *reset_btn;
};
ErrandsTimeChooser *errands_time_chooser_new();
// Get time string in HHMMSS format
const char *errands_time_chooser_get_time(ErrandsTimeChooser *tc);
void errands_time_chooser_set_time(ErrandsTimeChooser *tc, const char *hhmmss);
void errands_time_chooser_reset(ErrandsTimeChooser *tc);

struct _ErrandsWeekChooser {
  GtkListBoxRow parent_instance;
  GtkWidget *week_box;
};
ErrandsWeekChooser *errands_week_chooser_new();
const char *errands_week_chooser_get_days(ErrandsWeekChooser *wc);
void errands_week_chooser_set_days(ErrandsWeekChooser *wc, const char *rrule);
void errands_week_chooser_reset(ErrandsWeekChooser *wc);

struct _ErrandsDateWindow {
  AdwDialog parent_instance;
  GtkWidget *start_time_row;
  ErrandsTimeChooser *start_time_chooser;
  GtkWidget *start_date_row;
  ErrandsDateChooser *start_date_chooser;
  GtkWidget *repeat_row;
  GtkWidget *frequency_row;
  ErrandsWeekChooser *week_chooser;
  ErrandsMonthChooser *month_chooser;
  GtkWidget *count_row;
  GtkWidget *interval_row;
  GtkWidget *until_row;
  ErrandsDateChooser *until_date_chooser;
  GtkWidget *due_time_row;
  ErrandsTimeChooser *due_time_chooser;
  GtkWidget *due_date_row;
  ErrandsDateChooser *due_date_chooser;
  ErrandsTask *task;
};
ErrandsDateWindow *errands_date_window_new();
void errands_date_window_show(ErrandsTask *task);

// --- COLOR WINDOW --- //

struct _ErrandsColorWindow {
  AdwDialog parent_instance;
  GtkWidget *color_box;
  ErrandsTask *task;
  bool block_signals;
};
ErrandsColorWindow *errands_color_window_new();
void errands_color_window_show(ErrandsTask *task);

// --- ATTACHMENTS WINDOW --- //

struct _ErrandsAttachmentsWindow {
  AdwDialog parent_instance;
  GtkWidget *title;
  GtkWidget *list_box;
  GtkWidget *placeholder;
  ErrandsTask *task;
};
ErrandsAttachmentsWindow *errands_attachments_window_new();
void errands_attachments_window_show(ErrandsTask *task);

// --- ATTACHMENTS WINDOW ROW --- //

struct _ErrandsAttachmentsWindowRow {
  AdwActionRow parent_instance;
  GtkWidget *del_btn;
  char *file_path;
};
ErrandsAttachmentsWindowRow *errands_attachments_window_row_new(const char *file_path);

// --- TAGS WINDOW --- //

struct _ErrandsTagsWindow {
  AdwDialog parent_instance;
  GtkWidget *entry;
  GtkWidget *list_box;
  GtkWidget *placeholder;
  ErrandsTask *task;
};
ErrandsTagsWindow *errands_tags_window_new();
void errands_tags_window_show(ErrandsTask *task);

// --- TAGS WINDOW TAG ROW --- //

struct _ErrandsTagsWindowRow {
  AdwActionRow parent_instance;
  GtkWidget *toggle;
  GtkWidget *del_btn;
};
ErrandsTagsWindowRow *errands_tags_window_row_new(const char *tag);

// --- NOTES WINDOW --- //

#include <gtksourceview/gtksource.h>

struct _ErrandsNotesWindow {
  AdwDialog parent_instance;
  GtkWidget *view;
  GtkSourceBuffer *buffer;
  GtkSourceLanguage *md_lang;
  GtkWidget *md_view;
  ErrandsTask *task;
};
ErrandsNotesWindow *errands_notes_window_new();
void errands_notes_window_show(ErrandsTask *task);

// --- PRIORITY WINDOW --- //

struct _ErrandsPriorityWindow {
  AdwDialog parent_instance;
  GtkWidget *high_row;
  GtkWidget *medium_row;
  GtkWidget *low_row;
  GtkWidget *none_row;
  GtkWidget *custom;
  ErrandsTask *task;
};
ErrandsPriorityWindow *errands_priority_window_new();
void errands_priority_window_show(ErrandsTask *task);
