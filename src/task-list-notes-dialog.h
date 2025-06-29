#pragma once

#include "task/task.h"

#include <adwaita.h>
#include <gtksourceview/gtksource.h>

#define ERRANDS_TYPE_TASK_LIST_NOTES_DIALOG (errands_task_list_notes_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListNotesDialog, errands_task_list_notes_dialog, ERRANDS, TASK_LIST_NOTES_DIALOG,
                     AdwDialog)

struct _ErrandsTaskListNotesDialog {
  AdwDialog parent_instance;
  GtkSourceBuffer *source_buffer;
  GtkSourceView *source_view;
  GtkWidget *md_view;

  ErrandsTask *current_task;
};

ErrandsTaskListNotesDialog *errands_task_list_notes_dialog_new();
void errands_task_list_notes_dialog_show(ErrandsTask *task);
