#pragma once

#include <adwaita.h>

// --- TOOLBAR --- //

#define ERRANDS_TYPE_TASK_TOOLBAR (errands_task_toolbar_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskToolbar, errands_task_toolbar, ERRANDS, TASK_TOOLBAR, AdwBin)

struct _ErrandsTaskToolbar {
  AdwBin parent_instance;
  struct _ErrandsTask *task; // Avoid circular import
  GtkWidget *date_btn;
  GtkWidget *notes_btn;
  GtkWidget *priority_btn;
  GtkWidget *tags_btn;
  GtkWidget *attachments_btn;
  GtkWidget *color_btn;
};

ErrandsTaskToolbar *errands_task_toolbar_new(struct _ErrandsTask *task);
void errands_task_toolbar_update_date_btn(ErrandsTaskToolbar *tb);

// --- DATE WINDOW --- //

#include "date-chooser.h"
#include "month-chooser.h"
#include "time-chooser.h"
#include "week-chooser.h"

#define ERRANDS_TYPE_DATE_WINDOW (errands_date_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsDateWindow, errands_date_window, ERRANDS, DATE_WINDOW, AdwDialog)

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

  struct _ErrandsTask *task;
};

ErrandsDateWindow *errands_date_window_new();
void errands_date_window_show(struct _ErrandsTask *task);

// --- COLOR WINDOW --- //

#define ERRANDS_TYPE_COLOR_WINDOW (errands_color_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsColorWindow, errands_color_window, ERRANDS, COLOR_WINDOW, AdwDialog)

struct _ErrandsColorWindow {
  AdwDialog parent_instance;
  GtkWidget *color_box;
  struct _ErrandsTask *task;
  bool block_signals;
};

ErrandsColorWindow *errands_color_window_new();
void errands_color_window_show(struct _ErrandsTask *task);

// --- ATTACHMENTS WINDOW --- //

#define ERRANDS_TYPE_ATTACHMENTS_WINDOW (errands_attachments_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsAttachmentsWindow, errands_attachments_window, ERRANDS, ATTACHMENTS_WINDOW, AdwDialog)

struct _ErrandsAttachmentsWindow {
  AdwDialog parent_instance;
  GtkWidget *title;
  GtkWidget *list_box;
  GtkWidget *placeholder;
  struct _ErrandsTask *task;
};

ErrandsAttachmentsWindow *errands_attachments_window_new();
void errands_attachments_window_show(struct _ErrandsTask *task);

// --- ATTACHMENTS WINDOW ROW --- //

#define ERRANDS_TYPE_ATTACHMENTS_WINDOW_ROW (errands_attachments_window_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsAttachmentsWindowRow, errands_attachments_window_row, ERRANDS, ATTACHMENTS_WINDOW_ROW,
                     AdwActionRow)

struct _ErrandsAttachmentsWindowRow {
  AdwActionRow parent_instance;
  GtkWidget *del_btn;
  GFile *file;
};

ErrandsAttachmentsWindowRow *errands_attachments_window_row_new(GFile *file);

// --- TAGS WINDOW --- //

#define ERRANDS_TYPE_TAGS_WINDOW (errands_tags_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTagsWindow, errands_tags_window, ERRANDS, TAGS_WINDOW, AdwDialog)

struct _ErrandsTagsWindow {
  AdwDialog parent_instance;
  GtkWidget *entry;
  GtkWidget *list_box;
  GtkWidget *placeholder;
  struct _ErrandsTask *task;
};

ErrandsTagsWindow *errands_tags_window_new();
void errands_tags_window_show(struct _ErrandsTask *task);

// --- TAGS WINDOW TAG ROW --- //

#define ERRANDS_TYPE_TAGS_WINDOW_ROW (errands_tags_window_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTagsWindowRow, errands_tags_window_row, ERRANDS, TAGS_WINDOW_ROW, AdwActionRow)

struct _ErrandsTagsWindowRow {
  AdwActionRow parent_instance;
  GtkWidget *toggle;
  GtkWidget *del_btn;
};

ErrandsTagsWindowRow *errands_tags_window_row_new(const char *tag);

// --- NOTES WINDOW --- //

#include <gtksourceview/gtksource.h>

#define ERRANDS_TYPE_NOTES_WINDOW (errands_notes_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsNotesWindow, errands_notes_window, ERRANDS, NOTES_WINDOW, AdwDialog)

struct _ErrandsNotesWindow {
  AdwDialog parent_instance;
  GtkWidget *view;
  GtkSourceBuffer *buffer;
  GtkSourceLanguage *md_lang;
  struct _ErrandsTask *task;
};

ErrandsNotesWindow *errands_notes_window_new();
void errands_notes_window_show(struct _ErrandsTask *task);

// --- PRIORITY WINDOW --- //

#define ERRANDS_TYPE_PRIORITY_WINDOW (errands_priority_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsPriorityWindow, errands_priority_window, ERRANDS, PRIORITY_WINDOW, AdwDialog)

struct _ErrandsPriorityWindow {
  AdwDialog parent_instance;
  GtkWidget *none_btn;
  GtkWidget *low_btn;
  GtkWidget *medium_btn;
  GtkWidget *high_btn;
  GtkWidget *custom;
  struct _ErrandsTask *task;
};

ErrandsPriorityWindow *errands_priority_window_new();
void errands_priority_window_show(struct _ErrandsTask *task);
