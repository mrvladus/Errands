#include "date-chooser.h"
#include "data.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_reset_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self);
static void on_today_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self);
static void on_tomorrow_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self);
static void on_time_preset_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self);

static void on_day_selected(ErrandsDateChooser *self);
static void on_time_set_cb(ErrandsDateChooser *self, GtkSpinButton *btn);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsDateChooser {
  AdwActionRow parent_instance;
  GtkCalendar *calendar;
  GtkButton *reset_btn;
  GtkPopover *date_popover;
  GtkPopover *time_popover;
  GtkSpinButton *hours, *minutes;

  icaltimetype dt;
};

G_DEFINE_TYPE(ErrandsDateChooser, errands_date_chooser, ADW_TYPE_ACTION_ROW)

static void errands_date_chooser_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_DATE_CHOOSER);
  G_OBJECT_CLASS(errands_date_chooser_parent_class)->dispose(gobject);
}

static void errands_date_chooser_class_init(ErrandsDateChooserClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_date_chooser_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/date-chooser.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsDateChooser, reset_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsDateChooser, date_popover);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsDateChooser, time_popover);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsDateChooser, calendar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsDateChooser, minutes);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsDateChooser, hours);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_date_chooser_reset);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_day_selected);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_time_set_cb);
}

static void errands_date_chooser_init(ErrandsDateChooser *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
  GSimpleActionGroup *ag = errands_add_action_group(self, "date-chooser");
  errands_add_action(ag, "reset", on_reset_action_cb, self, NULL);
  errands_add_action(ag, "today", on_today_action_cb, self, NULL);
  errands_add_action(ag, "tomorrow", on_tomorrow_action_cb, self, NULL);
  errands_add_action(ag, "time-preset", on_time_preset_action_cb, self, "i");
}

ErrandsDateChooser *errands_date_chooser_new() { return g_object_new(ERRANDS_TYPE_DATE_CHOOSER, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_date_chooser_reset(ErrandsDateChooser *self) {
  gtk_spin_button_set_value(self->hours, 0);
  gtk_spin_button_set_value(self->minutes, 0);
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  gtk_calendar_set_date(self->calendar, today);
  g_object_set(self, "subtitle", "", NULL);
  g_object_set(self, "title", _("Not Set"), NULL);
  g_object_set(self->reset_btn, "visible", false, NULL);
  self->dt = icaltime_null_date();
  self->dt.is_date = true;
}

icaltimetype errands_date_chooser_get_dt(ErrandsDateChooser *self) {
  self->dt = icaltime_normalize(self->dt);
  return self->dt;
}

void errands_date_chooser_set_dt(ErrandsDateChooser *self, const icaltimetype dt) {
  self->dt = dt;
  bool is_null = icaltime_is_null_date(dt);
  if (is_null) errands_date_chooser_reset(self);
  else {
    gtk_calendar_set_year(self->calendar, dt.year);
    gtk_calendar_set_month(self->calendar, dt.month - 1);
    gtk_calendar_set_day(self->calendar, dt.day);
    g_autoptr(GDateTime) date = gtk_calendar_get_date(self->calendar);
    g_autofree gchar *date_str = g_date_time_format(date, "%x");
    g_object_set(self, "subtitle", date_str, NULL);
    if (!dt.is_date) {
      gtk_spin_button_set_value(self->hours, dt.hour);
      gtk_spin_button_set_value(self->minutes, dt.minute);
      const char *time_str = tmp_str_printf("%02d:%02d", dt.hour, dt.minute);
      g_object_set(self, "title", is_null ? _("Not Set") : time_str, NULL);
    }
  }
  g_object_set(self->reset_btn, "visible", !is_null, NULL);
}

// ---------- ACTIONS ---------- //

static void on_reset_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self) {
  errands_date_chooser_reset(self);
}

static void on_today_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  gtk_calendar_set_date(self->calendar, tomorrow);
  gtk_calendar_set_date(self->calendar, today);
  gtk_popover_popdown(self->date_popover);
  icaltimetype today_date = icaltime_today();
  self->dt.year = today_date.year;
  self->dt.month = today_date.month;
  self->dt.day = today_date.day;
}

static void on_tomorrow_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  gtk_calendar_set_date(self->calendar, tomorrow);
  gtk_popover_popdown(self->date_popover);
  icaltimetype today_date = icaltime_today();
  icaltime_adjust(&today_date, 1, 0, 0, 0);
  self->dt.year = today_date.year;
  self->dt.month = today_date.month;
  self->dt.day = today_date.day;
}

static void on_time_preset_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self) {
  gtk_spin_button_set_value(self->minutes, 0);
  gtk_spin_button_set_value(self->hours, g_variant_get_int32(param));
}

// ---------- CALLBACKS ---------- //

static void on_day_selected(ErrandsDateChooser *self) {
  g_autoptr(GDateTime) date = gtk_calendar_get_date(self->calendar);
  g_autofree gchar *date_str = g_date_time_format(date, "%x");
  g_object_set(self, "subtitle", date_str, NULL);
  g_object_set(self->reset_btn, "visible", true, NULL);
  self->dt.year = g_date_time_get_year(date);
  self->dt.month = g_date_time_get_month(date);
  self->dt.day = g_date_time_get_day_of_month(date);
}

static void on_time_set_cb(ErrandsDateChooser *self, GtkSpinButton *btn) {
  int hour = gtk_spin_button_get_value_as_int(self->hours);
  int minute = gtk_spin_button_get_value_as_int(self->minutes);
  if (icaltime_is_null_date(self->dt)) on_today_action_cb(NULL, NULL, self);
  self->dt.is_date = false;
  self->dt.hour = hour;
  self->dt.minute = minute;
  self->dt.second = 0;
  gtk_editable_set_text(GTK_EDITABLE(self->hours), tmp_str_printf("%02d", hour));
  gtk_editable_set_text(GTK_EDITABLE(self->minutes), tmp_str_printf("%02d", minute));
  g_object_set(self, "title", tmp_str_printf("%02d:%02d", hour, minute), NULL);
  g_object_set(self->reset_btn, "visible", true, NULL);
}
