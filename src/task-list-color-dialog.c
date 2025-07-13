#include "task-list-color-dialog.h"
#include "state.h"
#include "task.h"
#include "utils.h"

static void on_dialog_close_cb(ErrandsTaskListColorDialog *self);
static void on_toggle_cb(ErrandsTaskListColorDialog *self, GtkCheckButton *btn);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListColorDialog {
  AdwDialog parent_instance;
  GtkWidget *color_box;
  ErrandsTask *current_task;
  bool block_signals;
};

G_DEFINE_TYPE(ErrandsTaskListColorDialog, errands_task_list_color_dialog, ADW_TYPE_DIALOG)

static void errands_task_list_color_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_COLOR_DIALOG);
  G_OBJECT_CLASS(errands_task_list_color_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_color_dialog_class_init(ErrandsTaskListColorDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_color_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-color-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListColorDialog, color_box);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_toggle_cb);
}

static void errands_task_list_color_dialog_init(ErrandsTaskListColorDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListColorDialog *errands_task_list_color_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_COLOR_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_color_dialog_show(ErrandsTask *task) {
  if (!state.main_window->task_list->color_dialog)
    state.main_window->task_list->color_dialog = errands_task_list_color_dialog_new();
  ErrandsTaskListColorDialog *dialog = state.main_window->task_list->color_dialog;
  dialog->block_signals = true;
  dialog->current_task = task;
  GPtrArray *colors = get_children(dialog->color_box);
  for (size_t i = 0; i < colors->len; i++) {
    const char *name = gtk_widget_get_name(colors->pdata[i]);
    const char *color = errands_data_get_str(task->data, DATA_PROP_COLOR);
    if (g_str_equal(name, color)) {
      gtk_check_button_set_active(GTK_CHECK_BUTTON(colors->pdata[i]), true);
      break;
    }
  }
  g_ptr_array_free(colors, false);
  dialog->block_signals = false;
  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(state.main_window));
}

// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskListColorDialog *self) {
  GPtrArray *colors = get_children(self->color_box);
  for (size_t i = 0; i < colors->len; i++) {
    GtkCheckButton *btn = GTK_CHECK_BUTTON(colors->pdata[i]);
    if (gtk_check_button_get_active(btn)) {
      const char *name = gtk_widget_get_name(GTK_WIDGET(btn));
      if (g_str_equal(name, errands_data_get_str(self->current_task->data, DATA_PROP_COLOR))) {
        adw_dialog_close(ADW_DIALOG(self));
        return;
      }
      const char *uid = errands_data_get_str(self->current_task->data, DATA_PROP_UID);
      LOG("Set accent color '%s' to task '%s'", name, uid);
      errands_data_set_str(self->current_task->data, DATA_PROP_COLOR, name);
      errands_task_update_accent_color(self->current_task);
      break;
    }
  }
  g_ptr_array_free(colors, false);
  errands_data_write_list(task_data_get_list(self->current_task->data));
  adw_dialog_close(ADW_DIALOG(self));
  // TODO: sync
}

static void on_toggle_cb(ErrandsTaskListColorDialog *self, GtkCheckButton *btn) {
  if (!self->block_signals && gtk_check_button_get_active(btn)) on_dialog_close_cb(self);
}
