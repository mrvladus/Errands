#include "task-list-date-dialog-date-chooser.h"
#include "data/data.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_month_set_cb(ErrandsTaskListDateDialogDateChooser *self);
static void on_select_btn_clicked_cb(ErrandsTaskListDateDialogDateChooser *self);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListDateDialogDateChooser {
  AdwActionRow parent_instance;
  GtkButton *reset_btn, *select_btn;
  GtkDropDown *month;
  GtkPopover *popover;
  GtkSpinButton *day, *year;
  icaltimetype date;
};

G_DEFINE_TYPE(ErrandsTaskListDateDialogDateChooser, errands_task_list_date_dialog_date_chooser, ADW_TYPE_ACTION_ROW)

static void errands_task_list_date_dialog_date_chooser_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER);
  G_OBJECT_CLASS(errands_task_list_date_dialog_date_chooser_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_date_chooser_class_init(ErrandsTaskListDateDialogDateChooserClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_date_chooser_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog-date-chooser.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogDateChooser, reset_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogDateChooser, popover);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogDateChooser, day);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogDateChooser, month);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogDateChooser, year);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogDateChooser, select_btn);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_month_set_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_select_btn_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_date_dialog_date_chooser_reset);
}

static void errands_task_list_date_dialog_date_chooser_init(ErrandsTaskListDateDialogDateChooser *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialogDateChooser *errands_task_list_date_dialog_date_chooser_new() {
  return g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER, NULL);
}

// ---------- PUBLIC FUNCTIONS ---------- //

icaltimetype errands_task_list_date_dialog_date_chooser_get_date(ErrandsTaskListDateDialogDateChooser *self) {
  return self->date;
}

void errands_task_list_date_dialog_date_chooser_set_date(ErrandsTaskListDateDialogDateChooser *self,
                                                         const icaltimetype date) {
  bool is_null = icaltime_is_null_date(date);
  if (is_null) self->date = icaltime_null_date();
  icaltimetype today = icaltime_today();
  gtk_spin_button_set_value(self->day, is_null ? today.day : date.day);
  gtk_adjustment_set_upper(
      gtk_spin_button_get_adjustment(self->day),
      icaltime_days_in_month(is_null ? today.month : date.month, is_null ? today.year : date.year));
  gtk_drop_down_set_selected(self->month, is_null ? today.month - 1 : date.month - 1);
  gtk_spin_button_set_value(self->year, is_null ? today.year : date.year);
  char dt[32];
  sprintf(dt, "%02d.%02d.%d", date.day, date.month, date.year);
  g_object_set(self, "subtitle", is_null ? _("Not Set") : dt, NULL);
  g_object_set(self->reset_btn, "visible", !is_null, NULL);
}

void errands_task_list_date_dialog_date_chooser_reset(ErrandsTaskListDateDialogDateChooser *self) {
  icaltimetype today = icaltime_today();
  gtk_spin_button_set_value(self->day, today.day);
  gtk_spin_button_set_value(self->year, today.year);
  gtk_drop_down_set_selected(self->month, today.month);
  self->date = icaltime_null_date();
  g_object_set(self, "subtitle", _("Not Set"), NULL);
  g_object_set(self->reset_btn, "visible", false, NULL);
}

// ---------- CALLBACKS ---------- //

static void on_month_set_cb(ErrandsTaskListDateDialogDateChooser *self) {
  size_t m = gtk_drop_down_get_selected(self->month) + 1;
  size_t y = gtk_spin_button_get_value_as_int(self->year);
  size_t days_in_month = icaltime_days_in_month(m, y);
  size_t curr_day = gtk_spin_button_get_value_as_int(self->day);
  gtk_adjustment_set_upper(gtk_spin_button_get_adjustment(self->day), days_in_month);
  if (curr_day > days_in_month) gtk_spin_button_set_value(self->day, days_in_month);
}

static void on_select_btn_clicked_cb(ErrandsTaskListDateDialogDateChooser *self) {
  self->date.year = gtk_spin_button_get_value_as_int(self->year);
  self->date.day = gtk_spin_button_get_value_as_int(self->day);
  self->date.month = gtk_drop_down_get_selected(self->month) + 1;
  char dt[32];
  sprintf(dt, "%02d.%02d.%d", self->date.day, self->date.month, self->date.year);
  g_object_set(self, "subtitle", dt, NULL);
  g_object_set(self->reset_btn, "visible", true, NULL);
  gtk_popover_popdown(self->popover);
}
