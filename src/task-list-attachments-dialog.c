#include "task-list-attachments-dialog.h"
#include "state.h"
#include "task-list-attachments-dialog-attachment.h"
#include "task.h"

static void on_dialog_close_cb(ErrandsTaskListAttachmentsDialog *self);
static void on_add_btn_clicked_cb();
static void on_attachment_clicked_cb(GtkListBox *box, ErrandsTaskListAttachmentsDialogAttachment *attachment);
static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y, gpointer data);

static void errands_task_list_attachments_dialog_add_attachment(ErrandsTaskListAttachmentsDialog *self,
                                                                const char *path);

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

void errands_task_list_attachments_dialog_update_ui(ErrandsTaskListAttachmentsDialog *self) {
  g_auto(GStrv) attachments = errands_data_get_strv(self->current_task->data, DATA_PROP_ATTACHMENTS);
  gtk_widget_set_visible(self->placeholder, (attachments ? g_strv_length(attachments) : 0) == 0);
}

void errands_task_list_attachments_dialog_show(ErrandsTask *task) {
  if (!state.main_window->task_list->attachments_dialog)
    state.main_window->task_list->attachments_dialog = errands_task_list_attachments_dialog_new();
  ErrandsTaskListAttachmentsDialog *self = state.main_window->task_list->attachments_dialog;
  self->current_task = task;
  // Remove rows
  gtk_list_box_remove_all(GTK_LIST_BOX(self->attachments_box));
  g_auto(GStrv) attachments = errands_data_get_strv(task->data, DATA_PROP_ATTACHMENTS);
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
  g_auto(GStrv) cur_attachments = errands_data_get_strv(self->current_task->data, DATA_PROP_ATTACHMENTS);
  // If already contains - return
  if (cur_attachments && g_strv_contains((const gchar *const *)cur_attachments, path)) return;
  // Add attachment
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  if (cur_attachments) g_strv_builder_addv(builder, (const char **)cur_attachments);
  g_strv_builder_add(builder, path);
  g_auto(GStrv) attachments = g_strv_builder_end(builder);
  errands_data_set_strv(self->current_task->data, DATA_PROP_ATTACHMENTS, attachments);
  ErrandsTaskListAttachmentsDialogAttachment *attachment = errands_task_list_attachments_dialog_attachment_new(path);
  gtk_list_box_append(GTK_LIST_BOX(self->attachments_box), GTK_WIDGET(attachment));
  errands_data_write_list(task_data_get_list(self->current_task->data));
  errands_task_list_attachments_dialog_update_ui(self);
}

// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskListAttachmentsDialog *self) {
  gtk_widget_remove_css_class(self->current_task->attachments_btn, "accent");
  g_auto(GStrv) attachments = errands_data_get_strv(self->current_task->data, DATA_PROP_ATTACHMENTS);
  if ((attachments ? g_strv_length(attachments) : 0) > 0)
    gtk_widget_add_css_class(self->current_task->attachments_btn, "accent");
}

static void __on_open_finish(GObject *obj, GAsyncResult *res, gpointer data) {
  g_autoptr(GFile) file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!file) return;
  g_autofree char *path = g_file_get_path(file);
  // TODO: get real path if under flatpak using getfattr ?
  errands_task_list_attachments_dialog_add_attachment(state.main_window->task_list->attachments_dialog, path);
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
