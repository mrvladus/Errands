#include "vendor/toolbox.h"
#include "widgets.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_time_set_cb(ErrandsTaskListDateDialogTimeChooser *self, GtkSpinButton *btn);
static void on_time_preset_cb(ErrandsTaskListDateDialogTimeChooser *self, GtkButton *btn);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListDateDialogTimeChooser {
  AdwActionRow parent_instance;
  GtkPopover *popover;
  GtkSpinButton *hours, *minutes;
  GtkButton *reset_btn;
  bool is_reset;
};

G_DEFINE_TYPE(ErrandsTaskListDateDialogTimeChooser, errands_task_list_date_dialog_time_chooser, ADW_TYPE_ACTION_ROW)

static void errands_task_list_date_dialog_time_chooser_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_TIME_CHOOSER);
  G_OBJECT_CLASS(errands_task_list_date_dialog_time_chooser_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_time_chooser_class_init(ErrandsTaskListDateDialogTimeChooserClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_time_chooser_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog-time-chooser.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogTimeChooser, hours);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogTimeChooser, minutes);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogTimeChooser, reset_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogTimeChooser, popover);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_date_dialog_time_chooser_reset);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_time_set_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_time_preset_cb);
}

static void errands_task_list_date_dialog_time_chooser_init(ErrandsTaskListDateDialogTimeChooser *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialogTimeChooser *errands_task_list_date_dialog_time_chooser_new() {
  return g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_TIME_CHOOSER, NULL);
}

// ---------- PUBLIC FUNCTIONS ---------- //

icaltimetype errands_task_list_date_dialog_time_chooser_get_time(ErrandsTaskListDateDialogTimeChooser *self) {
  icaltimetype time = ICALTIMETYPE_INITIALIZER;
  if (!self->is_reset) {
    time.hour = gtk_spin_button_get_value_as_int(self->hours);
    time.minute = gtk_spin_button_get_value_as_int(self->minutes);
  }
  return time;
}

void errands_task_list_date_dialog_time_chooser_set_time(ErrandsTaskListDateDialogTimeChooser *self,
                                                         const icaltimetype time) {
  gtk_spin_button_set_value(self->hours, time.hour);
  gtk_spin_button_set_value(self->minutes, time.minute);
  bool is_null = time.is_date;
  self->is_reset = is_null;
  char t[6];
  sprintf(t, "%02d:%02d", time.hour, time.minute);
  g_object_set(self, "subtitle", is_null ? _("Not Set") : t, NULL);
  g_object_set(self->reset_btn, "visible", !is_null, NULL);
  tb_log("Time Chooser: Set '%s'", is_null ? "NULL" : t);
}

void errands_task_list_date_dialog_time_chooser_reset(ErrandsTaskListDateDialogTimeChooser *self) {
  gtk_spin_button_set_value(self->hours, 0);
  gtk_spin_button_set_value(self->minutes, 0);
  g_object_set(self, "subtitle", _("Not Set"), NULL);
  g_object_set(self->reset_btn, "visible", false, NULL);
  self->is_reset = true;
}

// ---------- CALLBACKS ---------- //

static void on_time_set_cb(ErrandsTaskListDateDialogTimeChooser *self, GtkSpinButton *btn) {
  int hour = gtk_spin_button_get_value_as_int(self->hours);
  int minute = gtk_spin_button_get_value_as_int(self->minutes);
  char buf[3];
  sprintf(buf, "%02d", hour);
  gtk_editable_set_text(GTK_EDITABLE(self->hours), buf);
  sprintf(buf, "%02d", minute);
  gtk_editable_set_text(GTK_EDITABLE(self->minutes), buf);
  char time[6];
  sprintf(time, "%02d:%02d", hour, minute);
  g_object_set(self, "subtitle", time, NULL);
  g_object_set(self->reset_btn, "visible", true, NULL);
  self->is_reset = false;
}

static void on_time_preset_cb(ErrandsTaskListDateDialogTimeChooser *self, GtkButton *btn) {
  int hours = atoi(gtk_widget_get_name(GTK_WIDGET(btn)));
  gtk_spin_button_set_value(self->minutes, 0);
  gtk_spin_button_set_value(self->hours, hours);
}
