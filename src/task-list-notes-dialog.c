#include "task-list-notes-dialog.h"
#include "state.h"
#include "task.h"

#define HOEDOWN_IMPLEMENTATION
#include "vendor/hoedown.h"

#include <webkit/webkit.h>

static void on_dialog_close_cb(ErrandsTaskListNotesDialog *self);
static void on_text_changed_cb(ErrandsTaskListNotesDialog *self);
static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data);
static void on_web_view_decide_policy_cb(WebKitWebView *web_view, WebKitPolicyDecision *decision,
                                         WebKitPolicyDecisionType type, gpointer user_data);

G_DEFINE_TYPE(ErrandsTaskListNotesDialog, errands_task_list_notes_dialog, ADW_TYPE_DIALOG)

static void errands_task_list_notes_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_NOTES_DIALOG);
  G_OBJECT_CLASS(errands_task_list_notes_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_notes_dialog_class_init(ErrandsTaskListNotesDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_notes_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-notes-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListNotesDialog, source_buffer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListNotesDialog, source_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListNotesDialog, md_view);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_text_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_web_view_decide_policy_cb);
}

static void errands_task_list_notes_dialog_init(ErrandsTaskListNotesDialog *self) {
  gtk_source_init();
  gtk_widget_init_template(GTK_WIDGET(self));
  gtk_source_buffer_set_language(self->source_buffer, gtk_source_language_manager_get_language(
                                                          gtk_source_language_manager_get_default(), "markdown"));
  gtk_source_buffer_set_style_scheme(
      self->source_buffer,
      gtk_source_style_scheme_manager_get_scheme(
          gtk_source_style_scheme_manager_get_default(),
          adw_style_manager_get_dark(adw_style_manager_get_default()) ? "Adwaita-dark" : "Adwaita"));
  g_object_bind_property_full(adw_style_manager_get_default(), "dark", self->source_buffer, "style-scheme",
                              G_BINDING_SYNC_CREATE, on_style_toggled_cb, NULL, NULL, NULL);
  g_signal_connect_swapped(self->source_buffer, "changed", G_CALLBACK(on_text_changed_cb), self);
}

ErrandsTaskListNotesDialog *errands_task_list_notes_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_NOTES_DIALOG, NULL));
}

void errands_task_list_notes_dialog_show(ErrandsTask *task) {
  if (!state.main_window->task_list->notes_dialog)
    state.main_window->task_list->notes_dialog = errands_task_list_notes_dialog_new();
  ErrandsTaskListNotesDialog *dialog = state.main_window->task_list->notes_dialog;
  dialog->current_task = task;
  const char *notes = errands_data_get_str(task->data, DATA_PROP_NOTES);
  if (!notes) return;
  g_autofree gchar *text = gtk_source_utils_unescape_search_text(notes);
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(dialog->source_view)), text, -1);
  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(state.main_window));
  gtk_widget_grab_focus(GTK_WIDGET(dialog->source_view));
}

// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskListNotesDialog *self) {
  TaskData *data = self->current_task->data;
  GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->source_view));
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
  if (!g_str_equal(text, "")) gtk_widget_add_css_class(self->current_task->notes_btn, "accent");
  else gtk_widget_remove_css_class(self->current_task->notes_btn, "accent");
  adw_dialog_close(ADW_DIALOG(self));
  // TODO: sync
}

static void on_text_changed_cb(ErrandsTaskListNotesDialog *self) {
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->source_view));
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  g_autofree char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  // Parse MD and convert it to HTML
  hoedown_renderer *renderer = hoedown_html_renderer_new(0, 0);
  hoedown_document *document = hoedown_document_new(renderer, 0, 16);
  hoedown_buffer *buff = hoedown_buffer_new(strlen(text) * 10);
  hoedown_document_render(document, buff, (const uint8_t *)text, strlen(text));
  bool is_dark = adw_style_manager_get_dark(adw_style_manager_get_default());
  g_autofree gchar *outer_html = g_strdup_printf(
      "<body>"
      "<style>body{margin:0;padding:0;background-color:%s;color:%s}a{color:%s}</style>"
      "<div style='margin:6'>"
      "%s"
      "</div>"
      "</body>",
      is_dark ? "#242424" : "white", is_dark ? "white" : "black", is_dark ? "white" : "#blue", buff->data);
  webkit_web_view_load_html(WEBKIT_WEB_VIEW(self->md_view), outer_html, NULL);
  hoedown_buffer_free(buff);
  hoedown_document_free(document);
  hoedown_html_renderer_free(renderer);
}

static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data) {
  ErrandsTaskListNotesDialog *self = state.main_window->task_list->notes_dialog;
  if (!self) return false;
  gtk_source_buffer_set_style_scheme(
      self->source_buffer,
      gtk_source_style_scheme_manager_get_scheme(gtk_source_style_scheme_manager_get_default(),
                                                 g_value_get_boolean(from_value) ? "Adwaita-dark" : "Adwaita"));
  on_text_changed_cb(self);
  return false;
}

static void on_web_view_decide_policy_cb(WebKitWebView *web_view, WebKitPolicyDecision *decision,
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
