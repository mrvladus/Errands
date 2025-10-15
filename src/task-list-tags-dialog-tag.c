#include "data.h"
#include "settings.h"
#include "task-list.h"

static void on_toggle_cb(ErrandsTaskListTagsDialogTag *self, GtkCheckButton *btn);
static void on_delete_cb(ErrandsTaskListTagsDialogTag *self, GtkButton *btn);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListTagsDialogTag {
  AdwActionRow parent_instance;
  GtkWidget *check_btn;
};

G_DEFINE_TYPE(ErrandsTaskListTagsDialogTag, errands_task_list_tags_dialog_tag, ADW_TYPE_ACTION_ROW)

static void errands_task_list_tags_dialog_tag_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG_TAG);
  G_OBJECT_CLASS(errands_task_list_tags_dialog_tag_parent_class)->dispose(gobject);
}

static void errands_task_list_tags_dialog_tag_class_init(ErrandsTaskListTagsDialogTagClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_tags_dialog_tag_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-tags-dialog-tag.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListTagsDialogTag, check_btn);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_delete_cb);
}

static void errands_task_list_tags_dialog_tag_init(ErrandsTaskListTagsDialogTag *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListTagsDialogTag *errands_task_list_tags_dialog_tag_new(const char *tag, ErrandsTask *task) {
  ErrandsTaskListTagsDialogTag *self = g_object_new(ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG_TAG, NULL);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self), tag);
  g_auto(GStrv) tags = errands_data_get_strv(task->data, DATA_PROP_TAGS);
  const bool has_tag = tags && g_strv_contains((const gchar *const *)tags, tag);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(self->check_btn), has_tag);
  g_signal_connect_swapped(self->check_btn, "toggled", G_CALLBACK(on_toggle_cb), self);
  return self;
}

// ---------- CALLBACKS ---------- //

static void on_toggle_cb(ErrandsTaskListTagsDialogTag *self, GtkCheckButton *btn) {
  const char *tag = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(self));
  ErrandsTask *task = errands_task_list_tags_dialog_get_task(
      ERRANDS_TASK_LIST_TAGS_DIALOG(gtk_widget_get_ancestor(GTK_WIDGET(self), ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG)));
  gtk_check_button_get_active(btn) ? errands_data_add_tag(task->data, DATA_PROP_TAGS, tag)
                                   : errands_data_remove_tag(task->data, DATA_PROP_TAGS, tag);
  errands_data_write_list(task_data_get_list(task->data));
}

static void on_delete_cb(ErrandsTaskListTagsDialogTag *self, GtkButton *btn) {
  ErrandsTaskListTagsDialog *dialog =
      ERRANDS_TASK_LIST_TAGS_DIALOG(gtk_widget_get_ancestor(GTK_WIDGET(self), ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG));
  // Delete tag from lists and all tasks
  const char *tag = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(self));
  errands_settings_remove_tag(tag);
  // Delete tag widget row
  gtk_list_box_remove(GTK_LIST_BOX(gtk_widget_get_ancestor(GTK_WIDGET(self), GTK_TYPE_LIST_BOX)), GTK_WIDGET(self));
  // Update all tasks and remove tag from their tags
  // TODO: change model data here
  // GPtrArray *tasks = errands_task_list_get_all_tasks();
  // GPtrArray *lists_to_save = g_ptr_array_new();
  // for (size_t i = 0; i < tasks->len; i++) {
  //   ErrandsTask *task = tasks->pdata[i];
  //   g_auto(GStrv) task_tags = errands_data_get_strv(task->data, DATA_PROP_TAGS);
  //   if (g_strv_contains((const gchar *const *)task_tags, tag)) {
  //     errands_data_remove_tag(task->data, DATA_PROP_TAGS, tag);
  //     errands_task_update_tags(task);
  //     g_ptr_array_add(lists_to_save, task_data_get_list(task->data));
  //   }
  // }
  // for (size_t i = 0; i < lists_to_save->len; i++) errands_data_write_list(lists_to_save->pdata[i]);
  // g_ptr_array_free(tasks, false);
  // g_ptr_array_free(lists_to_save, false);
  errands_task_list_tags_dialog_update_ui(dialog);
}
