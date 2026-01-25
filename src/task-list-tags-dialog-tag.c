#include "data.h"
#include "settings.h"
#include "sync.h"
#include "task-list.h"

#include "vendor/toolbox.h"

static void on_toggle_cb(ErrandsTaskListTagsDialogTag *self, GtkCheckButton *btn);
static void on_delete_cb(ErrandsTaskListTagsDialogTag *self, GtkButton *btn);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListTagsDialogTag {
  AdwActionRow parent_instance;
  GtkWidget *check_btn;
  ErrandsTaskListTagsDialog *dialog;
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

ErrandsTaskListTagsDialogTag *errands_task_list_tags_dialog_tag_new(ErrandsTaskListTagsDialog *dialog, const char *tag,
                                                                    ErrandsTask *task) {
  ErrandsTaskListTagsDialogTag *self = g_object_new(ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG_TAG, NULL);
  self->dialog = dialog;
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self), tag);
  g_auto(GStrv) tags = errands_data_get_tags(task->data->ical);
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
  gtk_check_button_get_active(btn) ? errands_data_add_tag(task->data->ical, tag)
                                   : errands_data_remove_tag(task->data->ical, tag);
  errands_list_data_save(task->data->list);
  errands_sync_update_task(task->data);
}

static void on_delete_cb(ErrandsTaskListTagsDialogTag *self, GtkButton *btn) {
  // Delete tag from lists and all tasks
  const char *tag = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(self));
  LOG("Tags Dialog: Deleting tag: %s", tag);
  errands_settings_remove_tag(tag);
  for_range(i, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, i);
    g_autoptr(GPtrArray) tasks = g_ptr_array_sized_new(list->children->len);
    errands_list_data_get_flat_list(list, tasks);
    for_range(j, 0, tasks->len) {
      TaskData *task = g_ptr_array_index(tasks, j);
      if (errands_data_remove_tag(task->ical, tag)) errands_sync_update_task(task);
    }
  }
  // Delete tag widget row
  gtk_list_box_remove(GTK_LIST_BOX(gtk_widget_get_ancestor(GTK_WIDGET(self), GTK_TYPE_LIST_BOX)), GTK_WIDGET(self));
  errands_task_list_tags_dialog_update_ui(self->dialog);
}
