#include "state.h"
#include "sync.h"
#include "task-list.h"
#include "task.h"

#include "vendor/toolbox.h"

#include <gtksourceview/gtksource.h>

static void on_dialog_close_cb(ErrandsTaskListNotesDialog *self);
static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListNotesDialog {
  AdwDialog parent_instance;
  GtkSourceBuffer *source_buffer;
  GtkSourceView *source_view;

  ErrandsTask *current_task;
};

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
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
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
}

ErrandsTaskListNotesDialog *errands_task_list_notes_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_NOTES_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_notes_dialog_show(ErrandsTask *task) {
  if (!state.main_window->task_list->notes_dialog)
    state.main_window->task_list->notes_dialog = errands_task_list_notes_dialog_new();
  ErrandsTaskListNotesDialog *dialog = state.main_window->task_list->notes_dialog;
  LOG("Notes Dialog: Show");
  dialog->current_task = task;
  const char *notes = errands_data_get_str(task->data->data, DATA_PROP_NOTES);
  if (notes) {
    g_autofree gchar *text = gtk_source_utils_unescape_search_text(notes);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(dialog->source_view)), text, -1);
  }
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
  const char *notes = errands_data_get_str(data->data, DATA_PROP_NOTES);
  if (notes && !STR_EQUAL(text, notes)) {
    errands_data_set_str(data->data, DATA_PROP_NOTES, text);
    errands_data_write_list(data->list);
    errands_sync_schedule_task(data);
  } else if (!notes && text && !STR_EQUAL(text, "")) {
    errands_data_set_str(data->data, DATA_PROP_NOTES, text);
    errands_data_write_list(data->list);
    errands_sync_schedule_task(data);
  }
  errands_task_update_toolbar(self->current_task);
  adw_dialog_close(ADW_DIALOG(self));
}

static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data) {
  ErrandsTaskListNotesDialog *self = state.main_window->task_list->notes_dialog;
  if (!self) return false;
  gtk_source_buffer_set_style_scheme(
      self->source_buffer,
      gtk_source_style_scheme_manager_get_scheme(gtk_source_style_scheme_manager_get_default(),
                                                 g_value_get_boolean(from_value) ? "Adwaita-dark" : "Adwaita"));
  return false;
}
