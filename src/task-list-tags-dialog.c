#include "settings.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

static void on_dialog_close_cb(ErrandsTaskListTagsDialog *self);
static void on_entry_activated_cb(ErrandsTaskListTagsDialog *self, AdwEntryRow *entry);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListTagsDialog {
  AdwDialog parent_instance;
  GtkWidget *entry;
  GtkWidget *tags_box;
  GtkWidget *placeholder;
  ErrandsTask *current_task;
};

G_DEFINE_TYPE(ErrandsTaskListTagsDialog, errands_task_list_tags_dialog, ADW_TYPE_DIALOG)

static void errands_task_list_tags_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG);
  G_OBJECT_CLASS(errands_task_list_tags_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_tags_dialog_class_init(ErrandsTaskListTagsDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_tags_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-tags-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListTagsDialog, entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListTagsDialog, tags_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListTagsDialog, placeholder);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_entry_activated_cb);
}

static void errands_task_list_tags_dialog_init(ErrandsTaskListTagsDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListTagsDialog *errands_task_list_tags_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

ErrandsTask *errands_task_list_tags_dialog_get_task(ErrandsTaskListTagsDialog *self) { return self->current_task; }

void errands_task_list_tags_dialog_update_ui(ErrandsTaskListTagsDialog *self) {
  g_auto(GStrv) tags = errands_settings_get_tags();
  gtk_widget_set_visible(self->placeholder, (tags ? g_strv_length(tags) : 0) == 0);
}

void errands_task_list_tags_dialog_show(ErrandsTask *task) {
  if (!state.main_window->task_list->tags_dialog)
    state.main_window->task_list->tags_dialog = errands_task_list_tags_dialog_new();
  ErrandsTaskListTagsDialog *self = state.main_window->task_list->tags_dialog;
  self->current_task = task;
  // Add rows
  g_auto(GStrv) tags = errands_settings_get_tags();
  g_auto(GStrv) all_tags_no_dups = gstrv_remove_duplicates(tags);
  for (size_t i = 0; i < g_strv_length(all_tags_no_dups); i++) {
    ErrandsTaskListTagsDialogTag *row = errands_task_list_tags_dialog_tag_new(all_tags_no_dups[i], self->current_task);
    gtk_list_box_append(GTK_LIST_BOX(self->tags_box), GTK_WIDGET(row));
  }
  errands_task_list_tags_dialog_update_ui(self);
  // Show dialog
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
}

// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskListTagsDialog *self) {
  gtk_list_box_remove_all(GTK_LIST_BOX(self->tags_box));
  errands_task_update_tags(self->current_task);
}

static void on_entry_activated_cb(ErrandsTaskListTagsDialog *self, AdwEntryRow *entry) {
  const char *tag = string_trim((char *)gtk_editable_get_text(GTK_EDITABLE(entry)));
  // Return if empty
  if (STR_EQUAL(tag, "")) return;
  // Return if exists
  g_auto(GStrv) tags = errands_settings_get_tags();
  for (size_t i = 0; i < g_strv_length(tags); i++)
    if (STR_EQUAL(tag, tags[i])) return;
  // Add tag to current list data
  errands_settings_add_tag(tag);
  ErrandsTaskListTagsDialogTag *row = errands_task_list_tags_dialog_tag_new(tag, self->current_task);
  gtk_list_box_append(GTK_LIST_BOX(self->tags_box), GTK_WIDGET(row));
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  errands_task_list_tags_dialog_update_ui(self);
}
