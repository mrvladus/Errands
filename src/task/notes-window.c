#include "../data/data.h"
#include "../state.h"
#include "../utils.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "task.h"

#include <cmark.h>
#include <glib/gi18n.h>
#include <webkit/webkit.h>

static void on_errands_notes_window_close_cb(ErrandsNotesWindow *win, gpointer data);
static gboolean on_style_toggled(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data);
static void on_text_changed(ErrandsNotesWindow *win);
static void on_web_view_decide_policy(WebKitWebView *web_view, WebKitPolicyDecision *decision,
                                      WebKitPolicyDecisionType type, gpointer user_data);

G_DEFINE_TYPE(ErrandsNotesWindow, errands_notes_window, ADW_TYPE_DIALOG)

static void errands_notes_window_class_init(ErrandsNotesWindowClass *class) {}

static void errands_notes_window_init(ErrandsNotesWindow *self) {
  LOG("Creating notes window");

  g_object_set(self, "title", _("Notes"), "content-width", 600, "content-height", 600, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_notes_window_close_cb), NULL);

  // Header Bar
  GtkWidget *hb = adw_header_bar_new();

  // Toggle MD preview button
  GtkWidget *md_btn = gtk_toggle_button_new();
  g_object_set(md_btn, "icon-name", "errands-md-symbolic", "tooltip-text", _("Toggle Markdown View"), NULL);
  adw_header_bar_pack_start(ADW_HEADER_BAR(hb), md_btn);

  // Source view
  gtk_source_init();
  GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
  self->md_lang = gtk_source_language_manager_get_language(lm, "markdown");
  self->buffer = gtk_source_buffer_new(NULL);
  gtk_source_buffer_set_language(self->buffer, self->md_lang);
  self->view = gtk_source_view_new_with_buffer(self->buffer);
  g_object_set(self->view, "top-margin", 6, "bottom-margin", 6, "right-margin", 6, "left-margin", 6, "wrap-mode",
               GTK_WRAP_WORD, NULL);
  // Setup style scheme
  AdwStyleManager *s_mgr = adw_style_manager_get_default();
  GtkSourceStyleSchemeManager *sc_mgr = gtk_source_style_scheme_manager_get_default();
  if (adw_style_manager_get_dark(s_mgr))
    gtk_source_buffer_set_style_scheme(self->buffer,
                                       gtk_source_style_scheme_manager_get_scheme(sc_mgr, "Adwaita-dark"));
  else gtk_source_buffer_set_style_scheme(self->buffer, gtk_source_style_scheme_manager_get_scheme(sc_mgr, "Adwaita"));
  g_object_bind_property_full(adw_style_manager_get_default(), "dark", self->buffer, "style-scheme",
                              G_BINDING_SYNC_CREATE, on_style_toggled, NULL, NULL, NULL);
  g_signal_connect_swapped(self->buffer, "changed", G_CALLBACK(on_text_changed), self);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl), self->view);
  g_object_set(scrl, "vexpand", true, NULL);
  g_object_bind_property(md_btn, "active", scrl, "visible", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  // MD Preview
  self->md_view = webkit_web_view_new();
  g_signal_connect(self->md_view, "decide-policy", G_CALLBACK(on_web_view_decide_policy), NULL);
  g_object_bind_property(md_btn, "active", self->md_view, "visible", G_BINDING_SYNC_CREATE);

  // Box
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(box), scrl);
  gtk_box_append(GTK_BOX(box), self->md_view);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "content", box, "top-bar-style", ADW_TOOLBAR_RAISED, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsNotesWindow *errands_notes_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_NOTES_WINDOW, NULL));
}

void errands_notes_window_show(ErrandsTask *task) {
  if (!state.notes_window) state.notes_window = errands_notes_window_new();

  adw_dialog_present(ADW_DIALOG(state.notes_window), GTK_WIDGET(state.main_window));
  state.notes_window->task = task;
  const char *notes = errands_data_get_str(task->data, DATA_PROP_NOTES);
  if (!notes) return;
  g_autofree gchar *text = gtk_source_utils_unescape_search_text(notes);
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(state.notes_window->view)), text, -1);
  gtk_widget_grab_focus(state.notes_window->view);
}

// --- SIGNAL HANDLERS --- //

static void on_errands_notes_window_close_cb(ErrandsNotesWindow *win, gpointer _data) {
  TaskData *data = win->task->data;
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->view));
  // Get the start and end iterators of the text buffer
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buf, &start);
  gtk_text_buffer_get_end_iter(buf, &end);
  g_autofree char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
  // If text is different then save it
  const char *notes = errands_data_get_str(data, DATA_PROP_NOTES);
  if (notes && !g_str_equal(text, notes)) {
    errands_data_set_str(data, DATA_PROP_NOTES, text);
    errands_data_write_list(task_data_get_list(data));
  } else if (!notes && text && !g_str_equal(text, "")) {
    errands_data_set_str(data, DATA_PROP_NOTES, text);
    errands_data_write_list(task_data_get_list(data));
  }
  // Add css class to button if notes not empty
  if (!g_str_equal(text, "")) gtk_widget_add_css_class(win->task->toolbar->notes_btn, "accent");
  else gtk_widget_remove_css_class(win->task->toolbar->notes_btn, "accent");
}

static gboolean on_style_toggled(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data) {
  if (!state.notes_window) return false;
  // Set style scheme
  gtk_source_buffer_set_style_scheme(
      state.notes_window->buffer,
      gtk_source_style_scheme_manager_get_scheme(gtk_source_style_scheme_manager_get_default(),
                                                 g_value_get_boolean(from_value) ? "Adwaita-dark" : "Adwaita"));
  on_text_changed(state.notes_window);
  return false;
}

static void on_text_changed(ErrandsNotesWindow *win) {
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(win->view));
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  g_autofree char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
  cmark_parser_feed(parser, text, strlen(text));
  cmark_node *doc = cmark_parser_finish(parser);
  char *html = cmark_render_html(doc, CMARK_OPT_DEFAULT);
  bool is_dark = adw_style_manager_get_dark(adw_style_manager_get_default());
  g_autofree gchar *outer_html =
      g_strdup_printf("<body>"
                      "<style>body{margin:0;padding:0;background-color:%s;color:%s}a{color:%s}</style>"
                      "<div style='margin:6'>"
                      "%s"
                      "</div>"
                      "</body>",
                      is_dark ? "#242424" : "white", is_dark ? "white" : "black", is_dark ? "white" : "#blue", html);
  webkit_web_view_load_html(WEBKIT_WEB_VIEW(win->md_view), outer_html, NULL);
  free(html);
  cmark_node_free(doc);
  cmark_parser_free(parser);
}

static void on_web_view_decide_policy(WebKitWebView *web_view, WebKitPolicyDecision *decision,
                                      WebKitPolicyDecisionType type, gpointer user_data) {
  if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
    WebKitNavigationPolicyDecision *nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
    WebKitNavigationAction *nav_action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);
    WebKitURIRequest *request = webkit_navigation_action_get_request(nav_action);
    const char *uri = webkit_uri_request_get_uri(request);
    // If user clicked on url - open link in browser
    if (uri && strstr(uri, "http")) {
      g_autoptr(GtkUriLauncher) launcher = gtk_uri_launcher_new(uri);
      gtk_uri_launcher_launch(launcher, GTK_WINDOW(state.main_window), NULL, NULL, NULL);
      webkit_policy_decision_ignore(decision);
      return;
    }
  }
  webkit_policy_decision_use(decision);
}
