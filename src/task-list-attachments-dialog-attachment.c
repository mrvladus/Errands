#include "data.h"
#include "state.h"
#include "task-list.h"

#include "vendor/toolbox.h"

static void on_delete_cb(ErrandsTaskListAttachmentsDialogAttachment *self, GtkButton *btn);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListAttachmentsDialogAttachment {
  AdwActionRow parent_instance;
};

G_DEFINE_TYPE(ErrandsTaskListAttachmentsDialogAttachment, errands_task_list_attachments_dialog_attachment,
              ADW_TYPE_ACTION_ROW)

static void errands_task_list_attachments_dialog_attachment_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_ATTACHMENTS_DIALOG_ATTACHMENT);
  G_OBJECT_CLASS(errands_task_list_attachments_dialog_attachment_parent_class)->dispose(gobject);
}

static void
errands_task_list_attachments_dialog_attachment_class_init(ErrandsTaskListAttachmentsDialogAttachmentClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_attachments_dialog_attachment_dispose;
  gtk_widget_class_set_template_from_resource(
      GTK_WIDGET_CLASS(class), "/io/github/mrvladus/Errands/ui/task-list-attachments-dialog-attachment.ui");
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_delete_cb);
}

static void errands_task_list_attachments_dialog_attachment_init(ErrandsTaskListAttachmentsDialogAttachment *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListAttachmentsDialogAttachment *errands_task_list_attachments_dialog_attachment_new(const char *path) {
  ErrandsTaskListAttachmentsDialogAttachment *self =
      g_object_new(ERRANDS_TYPE_TASK_LIST_ATTACHMENTS_DIALOG_ATTACHMENT, NULL);
  g_autofree gchar *basename = g_path_get_basename(path);
  adw_action_row_set_subtitle(ADW_ACTION_ROW(self), path);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(self), basename);
  return self;
}

// ---------- CALLBACKS ---------- //

static void on_delete_cb(ErrandsTaskListAttachmentsDialogAttachment *self, GtkButton *btn) {
  ErrandsTask *task = errands_task_list_attachments_dialog_get_task(state.main_window->task_list->attachments_dialog);
  ErrandsTaskListAttachmentsDialog *dialog = ERRANDS_TASK_LIST_ATTACHMENTS_DIALOG(
      gtk_widget_get_ancestor(GTK_WIDGET(self), ERRANDS_TYPE_TASK_LIST_ATTACHMENTS_DIALOG));
  g_auto(GStrv) cur_attachments = errands_data_get_strv(task->data->data, DATA_PROP_ATTACHMENTS);
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for (size_t i = 0; i < g_strv_length(cur_attachments); i++)
    if (!STR_EQUAL(cur_attachments[i], adw_action_row_get_subtitle(ADW_ACTION_ROW(self))))
      g_strv_builder_add(builder, cur_attachments[i]);
  g_auto(GStrv) attachments = g_strv_builder_end(builder);
  errands_data_set_strv(task->data->data, DATA_PROP_ATTACHMENTS, attachments);
  // errands_data_write_list(task_data_get_list(task->data));
  gtk_list_box_remove(GTK_LIST_BOX(gtk_widget_get_ancestor(GTK_WIDGET(self), GTK_TYPE_LIST_BOX)), GTK_WIDGET(self));
  errands_task_list_attachments_dialog_update_ui(dialog);
}
