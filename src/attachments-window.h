#pragma once

#include "task.h"

// --- ATTACHMENTS WINDOW --- //

#define ERRANDS_TYPE_ATTACHMENTS_WINDOW (errands_attachments_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsAttachmentsWindow, errands_attachments_window,
                     ERRANDS, ATTACHMENTS_WINDOW, AdwDialog)

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

#define ERRANDS_TYPE_ATTACHMENTS_WINDOW_ROW                                    \
  (errands_attachments_window_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsAttachmentsWindowRow,
                     errands_attachments_window_row, ERRANDS,
                     ATTACHMENTS_WINDOW_ROW, AdwActionRow)

struct _ErrandsAttachmentsWindowRow {
  AdwActionRow parent_instance;
  GtkWidget *del_btn;
  GFile *file;
};

ErrandsAttachmentsWindowRow *errands_attachments_window_row_new(GFile *file);
