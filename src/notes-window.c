#include "notes-window.h"
#include "adwaita.h"
#include "data.h"
#include "glib-object.h"
#include "gtksourceview/gtksource.h"
#include "state.h"
#include "task.h"
#include "utils.h"

static void on_errands_notes_window_close_cb(ErrandsNotesWindow *win,
                                             gpointer data);
static gboolean on_style_toggled(GBinding *binding, const GValue *from_value,
                                 GValue *to_value, gpointer user_data);

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
  gtk_source_init();
  GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
  self->md_lang = gtk_source_language_manager_get_language(lm, "markdown");
  self->buffer = gtk_source_buffer_new(NULL);
  gtk_source_buffer_set_language(self->buffer, self->md_lang);
  self->view = gtk_source_view_new_with_buffer(self->buffer);
  g_object_set(self->view, "top-margin", 6, "bottom-margin", 6, "right-margin",
               6, "left-margin", 6, "wrap-mode", GTK_WRAP_WORD, NULL);
  // Setup style scheme
  AdwStyleManager *s_mgr = adw_style_manager_get_default();
  GtkSourceStyleSchemeManager *sc_mgr =
      gtk_source_style_scheme_manager_get_default();
  if (adw_style_manager_get_dark(s_mgr))
    gtk_source_buffer_set_style_scheme(
        self->buffer,
        gtk_source_style_scheme_manager_get_scheme(sc_mgr, "Adwaita-dark"));
  else
    gtk_source_buffer_set_style_scheme(
        self->buffer,
        gtk_source_style_scheme_manager_get_scheme(sc_mgr, "Adwaita"));
  g_object_bind_property_full(
      adw_style_manager_get_default(), "dark", self->buffer, "style-scheme",
      G_BINDING_SYNC_CREATE, on_style_toggled, NULL, NULL, NULL);

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

void errands_notes_window_show(ErrandsTask *task) {
  adw_dialog_present(ADW_DIALOG(state.notes_window), state.main_window);
  state.notes_window->task = task;
  gtk_text_buffer_set_text(
      gtk_text_view_get_buffer(GTK_TEXT_VIEW(state.notes_window->view)),
      task->data->notes, -1);
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
  if (strcmp(text, win->task->data->notes)) {
    free(win->task->data->notes);
    win->task->data->notes = strdup(text);
    errands_data_write();
  }
  g_free(text);

  // Add css class to button if notes not empty
  if (strcmp(win->task->data->notes, ""))
    gtk_widget_add_css_class(win->task->toolbar->notes_btn, "accent");
  else
    gtk_widget_remove_css_class(win->task->toolbar->notes_btn, "accent");
}

static gboolean on_style_toggled(GBinding *binding, const GValue *from_value,
                                 GValue *to_value, gpointer user_data) {
  if (!state.notes_window)
    return false;
  // Set style scheme
  GtkSourceStyleSchemeManager *mgr =
      gtk_source_style_scheme_manager_get_default();
  if (g_value_get_boolean(from_value))
    gtk_source_buffer_set_style_scheme(
        state.notes_window->buffer,
        gtk_source_style_scheme_manager_get_scheme(mgr, "Adwaita-dark"));
  else
    gtk_source_buffer_set_style_scheme(
        state.notes_window->buffer,
        gtk_source_style_scheme_manager_get_scheme(mgr, "Adwaita"));
  return false;
}
