// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "date-chooser.h"
#include "config.h"
#include "utils.h"

#include <glib/gi18n.h>

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

  ICalTime *dt;

  // Properties
  const char *date_string;
};

G_DEFINE_TYPE(ErrandsDateChooser, errands_date_chooser, ADW_TYPE_ACTION_ROW)

enum {
  PROP_0,

  PROP_DATE_STRING,

  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsDateChooser *self = ERRANDS_DATE_CHOOSER(object);
  switch (prop_id) {
  case PROP_DATE_STRING: g_value_set_string(value, self->date_string); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsDateChooser *self = ERRANDS_DATE_CHOOSER(object);
  switch (prop_id) {
  case PROP_DATE_STRING: self->date_string = g_value_get_string(value); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void errands_date_chooser_dispose(GObject *gobject) {
  ErrandsDateChooser *self = ERRANDS_DATE_CHOOSER(gobject);

  g_clear_object(&self->dt);
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_DATE_CHOOSER);

  G_OBJECT_CLASS(errands_date_chooser_parent_class)->dispose(gobject);
}

static void errands_date_chooser_class_init(ErrandsDateChooserClass *class) {
  GObjectClass *object_class = G_OBJECT_CLASS(class);
  object_class->dispose = errands_date_chooser_dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  obj_properties[PROP_DATE_STRING] = g_param_spec_string("date-string", "Date String", "Date string", NULL, G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/date-chooser.ui");
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

static void update_title(ErrandsDateChooser *self) {
  if (!self->dt || i_cal_time_is_null_time(self->dt) || i_cal_time_is_null_date(self->dt)) {
    g_object_set(self, "title", _("Not Set"), NULL);
    return;
  }
  g_autoptr(GDateTime) date = gtk_calendar_get_date(self->calendar);
  g_autofree gchar *date_str = g_date_time_format(date, "%x");
  if (i_cal_time_is_date(self->dt)) {
    g_object_set(self, "title", date_str, NULL);
    return;
  }
  int hour = i_cal_time_get_hour(self->dt);
  int minute = i_cal_time_get_minute(self->dt);
  g_autofree gchar *title = g_strdup_printf("%s, %02d:%02d", date_str, hour, minute);
  g_object_set(self, "title", title, NULL);
}

// ---------- PUBLIC FUNCTIONS ---------- //

ErrandsDateChooser *errands_date_chooser_new() { return g_object_new(ERRANDS_TYPE_DATE_CHOOSER, NULL); }

void errands_date_chooser_reset(ErrandsDateChooser *self) {
  g_clear_object(&self->dt);
  self->dt = i_cal_time_new_null_date();
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  gtk_calendar_set_date(self->calendar, today);
  gtk_spin_button_set_value(self->hours, 0);
  gtk_spin_button_set_value(self->minutes, 0);
  update_title(self);
  g_object_set(self->reset_btn, "visible", false, NULL);
}

ICalTime *errands_date_chooser_get_dt(ErrandsDateChooser *self) { return i_cal_time_clone(self->dt); }

void errands_date_chooser_set_dt(ErrandsDateChooser *self, ICalTime *dt) {
  g_clear_object(&self->dt);
  self->dt = dt ? i_cal_time_clone(dt) : i_cal_time_new_null_date();
  if (i_cal_time_is_null_time(self->dt) || i_cal_time_is_null_date(self->dt)) {
    errands_date_chooser_reset(self);
    return;
  }
  gtk_calendar_set_year(self->calendar, i_cal_time_get_year(self->dt));
  gtk_calendar_set_month(self->calendar, i_cal_time_get_month(self->dt) - 1);
  gtk_calendar_set_day(self->calendar, i_cal_time_get_day(self->dt));
  if (!i_cal_time_is_date(self->dt)) {
    gtk_spin_button_set_value(self->hours, i_cal_time_get_hour(self->dt));
    gtk_spin_button_set_value(self->minutes, i_cal_time_get_minute(self->dt));
  }
  update_title(self);
  g_object_set(self->reset_btn, "visible", true, NULL);
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
  g_clear_object(&self->dt);
  self->dt = i_cal_time_new_today();
}

static void on_tomorrow_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  gtk_calendar_set_date(self->calendar, tomorrow);
  gtk_popover_popdown(self->date_popover);
  g_clear_object(&self->dt);
  self->dt = i_cal_time_new_today();
  i_cal_time_adjust(self->dt, 1, 0, 0, 0);
}

static void on_time_preset_action_cb(GSimpleAction *action, GVariant *param, ErrandsDateChooser *self) {
  gtk_spin_button_set_value(self->minutes, 0);
  gtk_spin_button_set_value(self->hours, g_variant_get_int32(param));
}

// ---------- CALLBACKS ---------- //

static void on_day_selected(ErrandsDateChooser *self) {
  g_autoptr(GDateTime) date = gtk_calendar_get_date(self->calendar);
  i_cal_time_set_year(self->dt, g_date_time_get_year(date));
  i_cal_time_set_month(self->dt, g_date_time_get_month(date));
  i_cal_time_set_day(self->dt, g_date_time_get_day_of_month(date));
  update_title(self);
  g_object_set(self->reset_btn, "visible", true, NULL);
}

static void on_time_set_cb(ErrandsDateChooser *self, GtkSpinButton *btn) {
  int hour = gtk_spin_button_get_value_as_int(self->hours);
  int minute = gtk_spin_button_get_value_as_int(self->minutes);
  if (i_cal_time_is_null_date(self->dt)) on_today_action_cb(NULL, NULL, self);
  i_cal_time_set_is_date(self->dt, false);
  i_cal_time_set_hour(self->dt, hour);
  i_cal_time_set_minute(self->dt, minute);
  i_cal_time_set_second(self->dt, 0);
  gtk_editable_set_text(GTK_EDITABLE(self->hours), tmp_str_printf("%02d", hour));
  gtk_editable_set_text(GTK_EDITABLE(self->minutes), tmp_str_printf("%02d", minute));
  update_title(self);
  g_object_set(self->reset_btn, "visible", true, NULL);
}
