#include "data.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"
#include "task-list.h"
#include "task.h"
#include "utils.h"

#include <glib/gi18n.h>

static void on_edit_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_favorite_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_copy_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_export_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_cancel_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_delete_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_notes_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_tags_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_priority_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_date_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);
static void on_attachments_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self);

static void on_color_toggle_cb(ErrandsTaskMenu *self, GtkCheckButton *btn);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskMenu {
  GtkPopover parent_instance;
  GtkWidget *task_mode_box;
  GtkBox *color_box;

  ErrandsTask *task;
  bool block_signals;
};

G_DEFINE_TYPE(ErrandsTaskMenu, errands_task_menu, GTK_TYPE_POPOVER)

static void errands_task_menu_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_MENU);
  G_OBJECT_CLASS(errands_task_menu_parent_class)->dispose(gobject);
}

static void errands_task_menu_class_init(ErrandsTaskMenuClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_menu_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/io/github/mrvladus/Errands/ui/task-menu.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskMenu, color_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskMenu, task_mode_box);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_color_toggle_cb);
}

static void errands_task_menu_init(ErrandsTaskMenu *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
  GSimpleActionGroup *ag = errands_add_action_group(self, "task-menu");
  errands_add_action(ag, "edit", on_edit_action_cb, self, NULL);
  errands_add_action(ag, "favorite", on_favorite_action_cb, self, NULL);
  errands_add_action(ag, "copy", on_copy_action_cb, self, NULL);
  errands_add_action(ag, "export", on_export_action_cb, self, NULL);
  errands_add_action(ag, "cancel", on_cancel_action_cb, self, NULL);
  errands_add_action(ag, "delete", on_delete_action_cb, self, NULL);
  errands_add_action(ag, "date", on_date_action_cb, self, NULL);
  errands_add_action(ag, "notes", on_notes_action_cb, self, NULL);
  errands_add_action(ag, "tags", on_tags_action_cb, self, NULL);
  errands_add_action(ag, "priority", on_priority_action_cb, self, NULL);
  errands_add_action(ag, "attachments", on_attachments_action_cb, self, NULL);
}

ErrandsTaskMenu *errands_task_menu_new() { return g_object_new(ERRANDS_TYPE_TASK_MENU, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_menu_show(ErrandsTask *task, float x, float y, ErrandsTaskMenuMode mode) {
  ErrandsTaskList *task_list = state.main_window->task_list;
  ErrandsTaskMenu *self = task_list->task_menu;
  self->task = task;
  GdkRectangle rect = {x, y, 0, 0};
  gtk_popover_set_pointing_to(GTK_POPOVER(self), &rect);
  self->block_signals = true;
  // Set color
  g_autoptr(GPtrArray) colors = get_children(GTK_WIDGET(self->color_box));
  const char *color = errands_data_get_color(task->data->ical, false);
  if (!color) gtk_check_button_set_active(GTK_CHECK_BUTTON(colors->pdata[0]), true);
  else
    for (size_t i = 0; i < colors->len; i++) {
      const char *name = gtk_widget_get_name(colors->pdata[i]);
      if (STR_EQUAL(name, color)) gtk_check_button_set_active(GTK_CHECK_BUTTON(colors->pdata[i]), true);
    }
  // Set mode
  gtk_widget_set_visible(self->task_mode_box, mode == ERRANDS_TASK_MENU_MODE_TASK);

  self->block_signals = false;
  gtk_popover_popup(GTK_POPOVER(self));
}

// ---------- ACTIONS ---------- //

static void on_edit_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  gtk_editable_label_start_editing(GTK_EDITABLE_LABEL(self->task->edit_title));
}

static void on_favorite_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  gtk_widget_activate_action(GTK_WIDGET(self->task), "task.favorite", NULL, NULL);
}

static void on_copy_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
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

static void on_export_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  const char *filename = tmp_str_printf("%s.ics", errands_data_get_uid(self->task->data->ical));
  g_object_set(dialog, "initial-name", filename, NULL);
  gtk_file_dialog_save(dialog, GTK_WINDOW(state.main_window), NULL, on_export_finish_cb, self->task->data);
}

static void on_cancel_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  gtk_widget_activate_action(GTK_WIDGET(self->task), "task.cancel", NULL, NULL);
}

static void on_delete_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  errands_data_set_deleted(self->task->data->ical, true);
  errands_list_data_save(self->task->data->list);
  errands_window_add_toast(_("Task is Deleted"));
  errands_sync_delete_task(self->task->data);
  errands_sidebar_update_filter_rows();
  errands_sidebar_task_list_row_update(errands_sidebar_task_list_row_get(self->task->data->list));
}

static void on_notes_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  gtk_widget_activate_action(GTK_WIDGET(self->task), "task.notes", NULL, NULL);
}

static void on_tags_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  gtk_widget_activate_action(GTK_WIDGET(self->task), "task.tags", NULL, NULL);
}

static void on_priority_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  gtk_widget_activate_action(GTK_WIDGET(self->task), "task.priority", NULL, NULL);
}

static void on_date_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  gtk_widget_activate_action(GTK_WIDGET(self->task), "task.date", NULL, NULL);
}

static void on_attachments_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskMenu *self) {
  gtk_popover_popdown(GTK_POPOVER(self));
  gtk_widget_activate_action(GTK_WIDGET(self->task), "task.attachments", NULL, NULL);
}

// ---------- CALLBACKS ---------- //

static void on_color_toggle_cb(ErrandsTaskMenu *self, GtkCheckButton *btn) {
  if (self->block_signals || !gtk_check_button_get_active(btn)) return;
  g_autoptr(GPtrArray) colors = get_children(GTK_WIDGET(self->color_box));
  for (size_t i = 0; i < colors->len; i++) {
    GtkCheckButton *btn = GTK_CHECK_BUTTON(colors->pdata[i]);
    if (gtk_check_button_get_active(btn)) {
      const char *name = gtk_widget_get_name(GTK_WIDGET(btn));
      const char *curr_color = errands_data_get_color(self->task->data->ical, false);
      if (curr_color && STR_EQUAL(name, curr_color)) {
        adw_dialog_close(ADW_DIALOG(self));
        return;
      }
      const char *new_color = STR_EQUAL(name, "none") ? NULL : name;
      const char *uid = errands_data_get_uid(self->task->data->ical);
      LOG("Set accent color '%s' to task '%s'", new_color, uid);
      errands_data_set_color(self->task->data->ical, new_color, false);
      errands_task_update_accent_color(self->task);
      errands_sync_update_task(self->task->data);
      break;
    }
  }
  errands_list_data_save(self->task->data->list);
}
