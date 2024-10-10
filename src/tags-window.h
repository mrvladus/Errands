#pragma once

#include "task.h"

#include <adwaita.h>

// --- TAGS WINDOW --- //

#define ERRANDS_TYPE_TAGS_WINDOW (errands_tags_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTagsWindow, errands_tags_window, ERRANDS,
                     TAGS_WINDOW, AdwDialog)

struct _ErrandsTagsWindow {
  AdwDialog parent_instance;
  GtkWidget *list_box;
  GtkWidget *entry;
  ErrandsTask *task;
};

ErrandsTagsWindow *errands_tags_window_new();
void errands_tags_window_show(ErrandsTask *task);

// --- TAGS WINDOW TAG ROW --- //

#define ERRANDS_TYPE_TAGS_WINDOW_ROW (errands_tags_window_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTagsWindowRow, errands_tags_window_row, ERRANDS,
                     TAGS_WINDOW_ROW, AdwActionRow)

struct _ErrandsTagsWindowRow {
  AdwActionRow parent_instance;
  GtkWidget *toggle;
};

ErrandsTagsWindowRow *errands_tags_window_row_new(const char *tag);
