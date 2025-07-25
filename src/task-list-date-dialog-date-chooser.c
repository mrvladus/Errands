#include "data/data.h"
#include "utils.h"
#include "widgets.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_day_selected(ErrandsTaskListDateDialogDateChooser *self);
static void on_today_selected(ErrandsTaskListDateDialogDateChooser *self);
static void on_tomorrow_selected(ErrandsTaskListDateDialogDateChooser *self);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListDateDialogDateChooser {
  AdwActionRow parent_instance;
  GtkCalendar *calendar;
  GtkButton *reset_btn;
  GtkPopover *popover;
  bool is_reset;
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
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogDateChooser, calendar);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_day_selected);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_today_selected);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_tomorrow_selected);
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
  icaltimetype date = ICALTIMETYPE_INITIALIZER;
  date.is_date = true;
  if (!self->is_reset) {
    date.year = gtk_calendar_get_year(self->calendar);
    date.month = gtk_calendar_get_month(self->calendar) + 1;
    date.day = gtk_calendar_get_day(self->calendar);
  }
  LOG("Date Chooser: Get '%s'", icaltime_as_ical_string(date));
  return date;
}

void errands_task_list_date_dialog_date_chooser_set_date(ErrandsTaskListDateDialogDateChooser *self,
                                                         const icaltimetype date) {
  bool is_null = icaltime_is_null_date(date);
  self->is_reset = is_null;
  if (is_null) errands_task_list_date_dialog_date_chooser_reset(self);
  else {
    gtk_calendar_set_year(self->calendar, date.year);
    gtk_calendar_set_month(self->calendar, date.month - 1);
    gtk_calendar_set_day(self->calendar, date.day);
    g_autoptr(GDateTime) dt = gtk_calendar_get_date(self->calendar);
    g_autofree gchar *date_str = g_date_time_format(dt, "%x");
    g_object_set(self, "subtitle", date_str, NULL);
  }
  g_object_set(self->reset_btn, "visible", !is_null, NULL);
}

void errands_task_list_date_dialog_date_chooser_reset(ErrandsTaskListDateDialogDateChooser *self) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  gtk_calendar_select_day(self->calendar, today);
  g_object_set(self, "subtitle", _("Not Set"), NULL);
  g_object_set(self->reset_btn, "visible", false, NULL);
  self->is_reset = true;
}

// ---------- CALLBACKS ---------- //

static void on_today_selected(ErrandsTaskListDateDialogDateChooser *self) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  gtk_calendar_select_day(self->calendar, tomorrow);
  gtk_calendar_select_day(self->calendar, today);
  gtk_popover_popdown(self->popover);
  self->is_reset = false;
}

static void on_tomorrow_selected(ErrandsTaskListDateDialogDateChooser *self) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  gtk_calendar_select_day(self->calendar, tomorrow);
  gtk_popover_popdown(self->popover);
  self->is_reset = false;
}

static void on_day_selected(ErrandsTaskListDateDialogDateChooser *self) {
  g_autoptr(GDateTime) date = gtk_calendar_get_date(self->calendar);
  g_autofree gchar *date_str = g_date_time_format(date, "%x");
  g_object_set(self, "subtitle", date_str, NULL);
  g_object_set(self->reset_btn, "visible", true, NULL);
  self->is_reset = false;
  LOG("Date Chooser: Select date %s", date_str);
}
