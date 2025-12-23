#include "state.h"
#include "task-list.h"
#include "task.h"

static void on_dialog_close_cb(ErrandsTaskListAttachmentsDialog *self);
static void on_add_btn_clicked_cb();
static void on_attachment_clicked_cb(GtkListBox *box, ErrandsTaskListAttachmentsDialogAttachment *attachment);
static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y, gpointer data);

static void errands_task_list_attachments_dialog_add_attachment(ErrandsTaskListAttachmentsDialog *self,
                                                                const char *path);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListAttachmentsDialog {
  AdwDialog parent_instance;
  GtkWidget *attachments_box;
  GtkWidget *placeholder;
  ErrandsTask *current_task;
};

G_DEFINE_TYPE(ErrandsTaskListAttachmentsDialog, errands_task_list_attachments_dialog, ADW_TYPE_DIALOG)

static void errands_task_list_attachments_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_ATTACHMENTS_DIALOG);
  G_OBJECT_CLASS(errands_task_list_attachments_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_attachments_dialog_class_init(ErrandsTaskListAttachmentsDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_attachments_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-attachments-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListAttachmentsDialog, attachments_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListAttachmentsDialog, placeholder);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_add_btn_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_attachment_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_drop_cb);
}

static void errands_task_list_attachments_dialog_init(ErrandsTaskListAttachmentsDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListAttachmentsDialog *errands_task_list_attachments_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_ATTACHMENTS_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

ErrandsTask *errands_task_list_attachments_dialog_get_task(ErrandsTaskListAttachmentsDialog *self) {
  return self->current_task;
}

void errands_task_list_attachments_dialog_update_ui(ErrandsTaskListAttachmentsDialog *self) {
  g_auto(GStrv) attachments = errands_data_get_prop(self->current_task->data, PROP_ATTACHMENTS).sv;
  gtk_widget_set_visible(self->placeholder, (attachments ? g_strv_length(attachments) : 0) == 0);
}

void errands_task_list_attachments_dialog_show(ErrandsTask *task) {
  if (!state.main_window->task_list->attachments_dialog)
    state.main_window->task_list->attachments_dialog = errands_task_list_attachments_dialog_new();
  ErrandsTaskListAttachmentsDialog *self = state.main_window->task_list->attachments_dialog;
  self->current_task = task;
  // Remove rows
  gtk_list_box_remove_all(GTK_LIST_BOX(self->attachments_box));
  g_auto(GStrv) attachments = errands_data_get_prop(task->data, PROP_ATTACHMENTS).sv;
  // Add rows
  if (attachments)
    for (size_t i = 0; i < g_strv_length(attachments); i++) {
      ErrandsTaskListAttachmentsDialogAttachment *attachment =
          errands_task_list_attachments_dialog_attachment_new(attachments[i]);
      gtk_list_box_append(GTK_LIST_BOX(self->attachments_box), GTK_WIDGET(attachment));
    }
  errands_task_list_attachments_dialog_update_ui(self);
  // Show dialog
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
}

static void errands_task_list_attachments_dialog_add_attachment(ErrandsTaskListAttachmentsDialog *self,
                                                                const char *path) {
  // Get current attachments
  g_auto(GStrv) cur_attachments = errands_data_get_prop(self->current_task->data, PROP_ATTACHMENTS).sv;
  // If already contains - return
  if (cur_attachments && g_strv_contains((const gchar *const *)cur_attachments, path)) return;
  // Add attachment
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  if (cur_attachments) g_strv_builder_addv(builder, (const char **)cur_attachments);
  g_strv_builder_add(builder, path);
  g_auto(GStrv) attachments = g_strv_builder_end(builder);
  errands_data_set_prop(self->current_task->data, PROP_ATTACHMENTS, attachments);
  ErrandsTaskListAttachmentsDialogAttachment *attachment = errands_task_list_attachments_dialog_attachment_new(path);
  gtk_list_box_append(GTK_LIST_BOX(self->attachments_box), GTK_WIDGET(attachment));
  errands_list_data_save(self->current_task->data->as.task.list);
  errands_task_list_attachments_dialog_update_ui(self);
}

// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskListAttachmentsDialog *self) {
  errands_task_update_toolbar(self->current_task);
}

static void __on_open_finish(GObject *obj, GAsyncResult *res, gpointer data) {
  g_autoptr(GFile) file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!file) return;
  GFileInfo *info = g_file_query_info(file, "xattr::document-portal.host-path", G_FILE_QUERY_INFO_NONE, NULL, NULL);
  const char *real_path = g_file_info_get_attribute_as_string(info, "xattr::document-portal.host-path");
  g_autofree char *path = g_file_get_path(file);
  errands_task_list_attachments_dialog_add_attachment(state.main_window->task_list->attachments_dialog,
                                                      real_path ? real_path : path);
}

static void on_add_btn_clicked_cb() {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  gtk_file_dialog_open(dialog, GTK_WINDOW(state.main_window), NULL, __on_open_finish, NULL);
}

static void on_attachment_clicked_cb(GtkListBox *box, ErrandsTaskListAttachmentsDialogAttachment *attachment) {
  g_autoptr(GFile) file = g_file_new_for_path(adw_action_row_get_subtitle(ADW_ACTION_ROW(attachment)));
  g_autoptr(GtkFileLauncher) l = gtk_file_launcher_new(file);
  gtk_file_launcher_launch(l, GTK_WINDOW(state.main_window), NULL, NULL, NULL);
}

static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y, gpointer data) {
  g_autoptr(GFile) file = g_value_get_object(value);
  g_autofree gchar *path = g_file_get_path(file);
  errands_task_list_attachments_dialog_add_attachment(state.main_window->task_list->attachments_dialog, path);
  return true;
}
