#ifndef ERRANDS_NOTES_WINDOW_H
#define ERRANDS_NOTES_WINDOW_H

#include "data.h"

#include <adwaita.h>
#include <gtksourceview/gtksource.h>

#define ERRANDS_TYPE_NOTES_WINDOW (errands_notes_window_get_type())

G_DECLARE_FINAL_TYPE(ErrandsNotesWindow, errands_notes_window, ERRANDS,
                     NOTES_WINDOW, AdwDialog)

struct _ErrandsNotesWindow {
  AdwDialog parent_instance;
  GtkWidget *view;
  TaskData *data;
};

ErrandsNotesWindow *errands_notes_window_new();
void errands_notes_window_show(TaskData *data);

#endif // ERRANDS_NOTES_WINDOW_H
