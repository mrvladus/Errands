#include "notes-window.h"
#include "data.h"
#include "state.h"
#include "utils.h"

static void on_errands_notes_window_close_cb(ErrandsNotesWindow *win,
                                             gpointer data);

G_DEFINE_TYPE(ErrandsNotesWindow, errands_notes_window, ADW_TYPE_DIALOG)

static void errands_notes_window_class_init(ErrandsNotesWindowClass *class) {}

static void errands_notes_window_init(ErrandsNotesWindow *self) {
  LOG("Creating notes window");

  g_object_set(self, "title", "Notes", "content-width", 600, "content-height",
               600, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_notes_window_close_cb),
                   NULL);

  // Header Bar
  GtkWidget *hb = adw_header_bar_new();

  // Source view
  self->view = gtk_source_view_new();
  g_object_set(self->view, "top-margin", 6, "bottom-margin", 6, "right-margin",
               6, "left-margin", 6, "wrap-mode", GTK_WRAP_WORD, NULL);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl), self->view);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "content", scrl, "top-bar-style", ADW_TOOLBAR_RAISED, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsNotesWindow *errands_notes_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_NOTES_WINDOW, NULL));
}

void errands_notes_window_show(TaskData *data) {
  adw_dialog_present(ADW_DIALOG(state.notes_window), state.main_window);
  state.notes_window->data = data;
  gtk_text_buffer_set_text(
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(state.notes_window->view)),
      data->notes, -1);
}

// --- SIGNAL HANDLERS --- //

static void on_errands_notes_window_close_cb(ErrandsNotesWindow *win,
                                             gpointer data) {
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->view));

  // Get the start and end iterators of the text buffer
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buf, &start);
  gtk_text_buffer_get_end_iter(buf, &end);
  char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);

  // If text is different then save it
  if (strcmp(text, win->data->notes)) {
    free(win->data->notes);
    win->data->notes = strdup(text);
    errands_data_write();
  }
  g_free(text);
}
