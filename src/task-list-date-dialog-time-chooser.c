#include "task-list-date-dialog-time-chooser.h"
#include "gtk/gtk.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <libical/ical.h>
#include <stdbool.h>
#include <stdio.h>

static void on_time_set_cb(GtkSpinButton *btn);
static void on_time_preset_cb(ErrandsTaskListDateDialogTimeChooser *self, GtkButton *btn);
static void on_select_btn_clicked_cb(ErrandsTaskListDateDialogTimeChooser *self);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListDateDialogTimeChooser {
  AdwActionRow parent_instance;
  GtkPopover *popover;
  GtkSpinButton *hours;
  GtkSpinButton *minutes;
  GtkButton *reset_btn;
  icaltimetype time;
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
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_select_btn_clicked_cb);
}

static void errands_task_list_date_dialog_time_chooser_init(ErrandsTaskListDateDialogTimeChooser *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialogTimeChooser *errands_task_list_date_dialog_time_chooser_new() {
  return g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_TIME_CHOOSER, NULL);
}

// ---------- PUBLIC FUNCTIONS ---------- //

icaltimetype errands_task_list_date_dialog_time_chooser_get_time(ErrandsTaskListDateDialogTimeChooser *self) {
  return self->time;
}

void errands_task_list_date_dialog_time_chooser_set_time(ErrandsTaskListDateDialogTimeChooser *self,
                                                         icaltimetype time) {

  char m[3], h[3], t[6];
  sprintf(h, "%02d", time.hour);
  sprintf(m, "%02d", time.minute);
  sprintf(t, "%02d:%02d", time.hour, time.minute);
  gtk_editable_set_text(GTK_EDITABLE(self->hours), h);
  gtk_editable_set_text(GTK_EDITABLE(self->minutes), m);
  bool is_null = icaltime_is_null_time(time) || time.is_date;
  g_object_set(self, "subtitle", is_null ? _("Not Set") : t, NULL);
  g_object_set(self->reset_btn, "visible", !is_null, NULL);
  self->time = time;
}

void errands_task_list_date_dialog_time_chooser_reset(ErrandsTaskListDateDialogTimeChooser *self) {
  self->time = icaltime_null_time();
  gtk_spin_button_set_value(self->hours, 0);
  gtk_spin_button_set_value(self->minutes, 0);
  g_object_set(self, "subtitle", _("Not Set"), NULL);
  g_object_set(self->reset_btn, "visible", false, NULL);
}

// ---------- CALLBACKS ---------- //

static void on_time_set_cb(GtkSpinButton *btn) {
  char buf[3];
  sprintf(buf, "%02d", gtk_spin_button_get_value_as_int(btn));
  gtk_editable_set_text(GTK_EDITABLE(btn), buf);
}

static void on_time_preset_cb(ErrandsTaskListDateDialogTimeChooser *self, GtkButton *btn) {
  int hours = atoi(gtk_widget_get_name(GTK_WIDGET(btn)));
  gtk_spin_button_set_value(self->minutes, 0);
  gtk_spin_button_set_value(self->hours, hours);
  on_select_btn_clicked_cb(self);
}

static void on_select_btn_clicked_cb(ErrandsTaskListDateDialogTimeChooser *self) {
  self->time.hour = gtk_spin_button_get_value_as_int(self->hours);
  self->time.minute = gtk_spin_button_get_value_as_int(self->minutes);
  char time[6];
  sprintf(time, "%02d:%02d", self->time.hour, self->time.minute);
  g_object_set(self, "subtitle", time, NULL);
  g_object_set(self->reset_btn, "visible", true, NULL);
  gtk_popover_popdown(self->popover);
}
