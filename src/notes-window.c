#include "notes-window.h"
#include "data.h"
#include "state.h"

#include <adwaita.h>
#include <gtksourceview/gtksource.h>
#include <stdlib.h>
#include <string.h>

static void on_errands_notes_window_close_cb(AdwDialog *win, gpointer data);

// --- LOCAL STATE --- //

GtkWidget *notes_window_source_view;
char *notes_window_uid;

// --- FUNCTIONS IMPLEMENTATIONS --- //

void errands_notes_window_build() {
  LOG("Creating notes window");

  // Header Bar
  GtkWidget *hb = adw_header_bar_new();

  // Source view
  notes_window_source_view = gtk_source_view_new();
  g_object_set(notes_window_source_view, "top-margin", 6, "bottom-margin", 6,
               "right-margin", 6, "left-margin", 6, "wrap-mode", GTK_WRAP_WORD,
               NULL);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl),
                                notes_window_source_view);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "content", scrl, "top-bar-style", ADW_TOOLBAR_RAISED, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);

  // Window
  state.notes_window = g_object_ref_sink(adw_dialog_new());
  g_object_set(state.notes_window, "child", tb, "title", "Notes",
               "content-width", 600, "content-height", 600, NULL);
  g_signal_connect(state.notes_window, "closed",
                   G_CALLBACK(on_errands_notes_window_close_cb), NULL);
}

void errands_notes_window_show(TaskData *td) {
  adw_dialog_present(state.notes_window, state.main_window);
  notes_window_uid = td->uid;
  GtkTextBuffer *buf =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(notes_window_source_view));
  gtk_text_buffer_set_text(buf, td->notes, -1);
}

// --- SIGNAL HANDLERS --- //

static void on_errands_notes_window_close_cb(AdwDialog *win, gpointer data) {
  TaskData *td = errands_data_get_task(notes_window_uid);
  GtkTextBuffer *buf =
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(notes_window_source_view));

  // Get the start and end iterators of the text buffer
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buf, &start);
  gtk_text_buffer_get_end_iter(buf, &end);
  char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);

  // If text is different then save it
  if (strcmp(text, td->notes)) {
    free(td->notes);
    td->notes = strdup(text);
    errands_data_write();
  }
  g_free(text);
}
