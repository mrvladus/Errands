#include "task-properties-dialog.h"
#include "data.h"
#include "state.h"
#include "sync.h"

#include <gtksourceview/gtksource.h>

static void on_dialog_close_cb(ErrandsTaskPropertiesDialog *self);
static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data);

static ErrandsTaskPropertiesDialog *self = NULL;
static char *page_names[] = {"date", "notes", "priority", "attachments", "tags"};

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskPropertiesDialog {
  AdwDialog parent_instance;
  AdwToolbarView *tbv;
  AdwViewStack *stack;
  GtkSourceView *notes_view;
  GtkSourceBuffer *notes_buffer;

  ErrandsTask *task;
};

G_DEFINE_TYPE(ErrandsTaskPropertiesDialog, errands_task_properties_dialog, ADW_TYPE_DIALOG)

static void errands_task_properties_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_PROPERTIES_DIALOG);
  G_OBJECT_CLASS(errands_task_properties_dialog_parent_class)->dispose(gobject);
}

static void errands_task_properties_dialog_class_init(ErrandsTaskPropertiesDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_properties_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-properties-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, tbv);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, stack);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, notes_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, notes_buffer);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
}

static void errands_task_properties_dialog_init(ErrandsTaskPropertiesDialog *dialog) {
  gtk_widget_init_template(GTK_WIDGET(dialog));

  // Notes
  gtk_source_init();
  GtkSourceLanguageManager *lang_mgr = gtk_source_language_manager_get_default();
  gtk_source_buffer_set_language(dialog->notes_buffer, gtk_source_language_manager_get_language(lang_mgr, "markdown"));
  AdwStyleManager *style_mgr = adw_style_manager_get_default();
  const char *theme = adw_style_manager_get_dark(style_mgr) ? "Adwaita-dark" : "Adwaita";
  GtkSourceStyleSchemeManager *style_scheme_mgr = gtk_source_style_scheme_manager_get_default();
  GtkSourceStyleScheme *style_scheme = gtk_source_style_scheme_manager_get_scheme(style_scheme_mgr, theme);
  gtk_source_buffer_set_style_scheme(dialog->notes_buffer, style_scheme);
  g_object_bind_property_full(style_mgr, "dark", dialog->notes_buffer, "style-scheme", G_BINDING_SYNC_CREATE,
                              on_style_toggled_cb, NULL, NULL, NULL);
}

ErrandsTaskPropertiesDialog *errands_task_properties_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_PROPERTIES_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_properties_dialog_show(ErrandsTaskPropertiesDialogPage page, ErrandsTask *task) {
  if (!self) self = errands_task_properties_dialog_new();
  self->task = task;
  int page_n = CLAMP(page, 0, ERRANDS_TASK_PROPERTY_DIALOG_N_PAGES - 1);
  adw_view_stack_set_visible_child_name(self->stack, page_names[page_n]);

  // Notes
  const char *notes = errands_data_get_notes(task->data->ical);
  if (notes) {
    g_autofree gchar *text = gtk_source_utils_unescape_search_text(notes);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->notes_view)), text, -1);
  } else gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->notes_view)), "", -1);
  if (page_n == ERRANDS_TASK_PROPERTY_DIALOG_PAGE_NOTES) gtk_widget_grab_focus(GTK_WIDGET(self->notes_view));

  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
}

// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskPropertiesDialog *self) {
  TaskData *data = self->task->data;
  bool changed = false;

  // Notes
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(self->notes_buffer), &start);
  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(self->notes_buffer), &end);
  g_autofree char *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(self->notes_buffer), &start, &end, FALSE);
  const char *notes = errands_data_get_notes(data->ical);
  if (text && (!notes || !STR_EQUAL(text, notes))) {
    errands_data_set_notes(data->ical, text);
    changed = true;
  }

  if (changed) {
    errands_list_data_save(data->list);
    errands_task_update_toolbar(self->task);
    errands_sync_update_task(data);
  }
}

static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data) {
  if (!self) return false;
  GtkSourceStyleSchemeManager *style_scheme_mgr = gtk_source_style_scheme_manager_get_default();
  const char *theme = g_value_get_boolean(from_value) ? "Adwaita-dark" : "Adwaita";
  GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(style_scheme_mgr, theme);
  gtk_source_buffer_set_style_scheme(self->notes_buffer, scheme);

  return false;
}
