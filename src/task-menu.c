#include "data.h"
#include "state.h"
#include "sync.h"
#include "task-list.h"
#include "task.h"

#include <glib/gi18n.h>

static void on_edit_clicked_cb(ErrandsTaskMenu *self);
static void on_clipboard_clicked_cb(ErrandsTaskMenu *self);
static void on_color_clicked_cb(ErrandsTaskMenu *self);
static void on_export_clicked_cb(ErrandsTaskMenu *self);
static void on_delete_clicked_cb(ErrandsTaskMenu *self);
static void on_cancel_clicked_cb(ErrandsTaskMenu *self);
static void on_tags_clicked_cb(ErrandsTaskMenu *self);
static void on_attachments_clicked_cb(ErrandsTaskMenu *self);
static void on_notes_clicked_cb(ErrandsTaskMenu *self);
static void on_priority_clicked_cb(ErrandsTaskMenu *self);
static void on_date_clicked_cb(ErrandsTaskMenu *self);
static void on_pin_clicked_cb(ErrandsTaskMenu *self);
static void on_subtasks_clicked_cb(ErrandsTaskMenu *self);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskMenu {
  GtkPopover parent_instance;
  ErrandsTask *task;
};

G_DEFINE_TYPE(ErrandsTaskMenu, errands_task_menu, GTK_TYPE_POPOVER)

static void errands_task_menu_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_MENU);
  G_OBJECT_CLASS(errands_task_menu_parent_class)->dispose(gobject);
}

static void errands_task_menu_class_init(ErrandsTaskMenuClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_menu_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/io/github/mrvladus/Errands/ui/task-menu.ui");
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_subtasks_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_edit_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_clipboard_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_export_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_delete_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_cancel_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_color_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_tags_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_attachments_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_notes_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_priority_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_date_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_pin_clicked_cb);
}

static void errands_task_menu_init(ErrandsTaskMenu *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsTaskMenu *errands_task_menu_new() { return g_object_new(ERRANDS_TYPE_TASK_MENU, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_menu_show(ErrandsTask *task) {
  ErrandsTaskList *task_list = state.main_window->task_list;
  ErrandsTaskMenu *self = task_list->task_menu;
  self->task = task;
  GdkRectangle rect = {task_list->x, task_list->y, 0, 0};
  gtk_popover_set_pointing_to(GTK_POPOVER(self), &rect);
  gtk_popover_popup(GTK_POPOVER(self));
}

// ---------- CALLBACKS ---------- //

static void on_delete_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  errands_data_set_deleted(self->task->data->ical, true);
  errands_list_data_save(self->task->data->list);
  errands_task_list_reload(state.main_window->task_list, true);
  errands_window_add_toast(_("Task is Deleted"));
  errands_sync_schedule_task(self->task->data);
}

static void on_edit_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  gtk_editable_label_start_editing(GTK_EDITABLE_LABEL(self->task->edit_title));
}

static void on_clipboard_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  const char *text = errands_data_get_text(self->task->data->ical);
  GdkClipboard *clipboard = gdk_display_get_clipboard(gtk_widget_get_display(GTK_WIDGET(self)));
  gdk_clipboard_set(clipboard, G_TYPE_STRING, text);
  errands_window_add_toast(_("Copied to Clipboard"));
}

static void on_export_finish_cb(GObject *obj, GAsyncResult *res, gpointer data) {
  g_autoptr(GFile) f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!f) return;
  g_autofree char *path = g_file_get_path(f);
  FILE *file = fopen(path, "w");
  if (!file) {
    errands_window_add_toast(_("Failed to Export"));
    return;
  }
  TaskData *task_data = data;
  autoptr(icalcomponent) cal = icalcomponent_new_vcalendar();
  autoptr(icalcomponent) dup = icalcomponent_new_clone(task_data->ical);
  icalcomponent_add_component(cal, dup);
  autofree char *ical = icalcomponent_as_ical_string(cal);
  fprintf(file, "%s", ical);
  fclose(file);
  LOG("Exported Task to '%s'", path);
}

static void on_export_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  const char *filename = tmp_str_printf("%s.ics", errands_data_get_uid(self->task->data->ical));
  g_object_set(dialog, "initial-name", filename, NULL);
  gtk_file_dialog_save(dialog, GTK_WINDOW(state.main_window), NULL, on_export_finish_cb, self->task->data);
}

static void on_cancel_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  errands_data_set_cancelled(self->task->data->ical, true);
  g_autoptr(GPtrArray) sub_tasks = g_ptr_array_new();
  errands_task_data_get_flat_list(self->task->data, sub_tasks);
  for_range(i, 0, sub_tasks->len) {
    TaskData *sub_task = g_ptr_array_index(sub_tasks, i);
    errands_data_set_cancelled(sub_task->ical, true);
  }
  errands_list_data_save(self->task->data->list);
  errands_task_list_reload(state.main_window->task_list, true);
}

static void on_color_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  errands_task_list_color_dialog_show(self->task);
}

static void on_tags_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  errands_task_list_tags_dialog_show(self->task);
}

static void on_attachments_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  errands_task_list_attachments_dialog_show(self->task);
}

static void on_notes_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  errands_task_list_notes_dialog_show(self->task);
}

static void on_priority_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  errands_task_list_priority_dialog_show(self->task);
}

static void on_date_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  errands_task_list_date_dialog_show(self->task);
}

static void on_pin_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  bool new_pinned = !errands_data_get_pinned(self->task->data->ical);
  errands_data_set_pinned(self->task->data->ical, new_pinned);
  errands_list_data_save(self->task->data->list);
  errands_sidebar_update_filter_rows(state.main_window->sidebar);
  errands_task_list_reload(state.main_window->task_list, true);
}

static void on_subtasks_clicked_cb(ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  bool new_expanded = !errands_data_get_expanded(self->task->data->ical);
  errands_data_set_expanded(self->task->data->ical, new_expanded);
  errands_list_data_save(self->task->data->list);
  errands_task_list_reload(state.main_window->task_list, true);
  gtk_widget_grab_focus(self->task->sub_entry);
}
