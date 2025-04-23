#include "task.h"

#include <glib/gi18n.h>

static void on_today_btn_clicked_cb(ErrandsDateChooser *dc);
static void on_tomorrow_btn_clicked_cb(ErrandsDateChooser *dc);
static void on_date_set_cb(ErrandsDateChooser *dc);

G_DEFINE_TYPE(ErrandsDateChooser, errands_date_chooser, GTK_TYPE_BOX)

static void errands_date_chooser_class_init(ErrandsDateChooserClass *class) {}

static void errands_date_chooser_init(ErrandsDateChooser *self) {
  g_object_set(self, "spacing", 6, NULL);

  // Today btn
  GtkWidget *today_btn = gtk_button_new_with_label(_("Today"));
  gtk_widget_add_css_class(today_btn, "flat");
  g_signal_connect_swapped(today_btn, "clicked", G_CALLBACK(on_today_btn_clicked_cb), self);

  // Tomorrow btn
  GtkWidget *tomorrow_btn = gtk_button_new_with_label(_("Tomorrow"));
  gtk_widget_add_css_class(tomorrow_btn, "flat");
  g_signal_connect_swapped(tomorrow_btn, "clicked", G_CALLBACK(on_tomorrow_btn_clicked_cb), self);

  GtkWidget *preset_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_set(preset_box, "homogeneous", true, NULL);
  gtk_box_append(GTK_BOX(preset_box), today_btn);
  gtk_box_append(GTK_BOX(preset_box), tomorrow_btn);

  // Calendar
  self->calendar = gtk_calendar_new();
  g_signal_connect_swapped(self->calendar, "day-selected", G_CALLBACK(on_date_set_cb), self);

  // Popover box
  GtkWidget *popover_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_append(GTK_BOX(popover_box), preset_box);
  gtk_box_append(GTK_BOX(popover_box), self->calendar);

  // Popover
  GtkWidget *popover = gtk_popover_new();
  g_object_set(popover, "child", popover_box, NULL);

  // Button
  GtkWidget *btn = g_object_new(GTK_TYPE_MENU_BUTTON, "popover", popover, "icon-name", "errands-calendar-symbolic",
                                "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(btn, "flat");

  // Label
  self->label = gtk_label_new(_("Not Set"));
  gtk_widget_add_css_class(self->label, "numeric");

  // Reset button
  self->reset_btn = g_object_new(GTK_TYPE_BUTTON, "icon-name", "errands-reset-symbolic", "tooltip-text", _("Reset"),
                                 "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(self->reset_btn, "flat");
  g_signal_connect_swapped(self->reset_btn, "clicked", G_CALLBACK(errands_date_chooser_reset), self);

  gtk_box_append(GTK_BOX(self), self->reset_btn);
  gtk_box_append(GTK_BOX(self), self->label);
  gtk_box_append(GTK_BOX(self), btn);
}

ErrandsDateChooser *errands_date_chooser_new() { return g_object_new(ERRANDS_TYPE_DATE_CHOOSER, NULL); }

const char *errands_date_chooser_get_date(ErrandsDateChooser *dc) {
  g_autoptr(GDateTime) datetime = gtk_calendar_get_date(GTK_CALENDAR(dc->calendar));
  g_autofree gchar *date_str = g_date_time_format(datetime, "%Y%m%d");
  static char date[9];
  strcpy(date, date_str);
  return date;
}

void errands_date_chooser_set_date(ErrandsDateChooser *dc, const char *date) {
  g_autoptr(GDateTime) dt;
  if (!strstr(date, "T")) {
    char new_date[16];
    sprintf(new_date, "%sT000000Z", date);
    dt = g_date_time_new_from_iso8601(new_date, NULL);
  } else dt = g_date_time_new_from_iso8601(date, NULL);
  if (dt) {
    g_autoptr(GDateTime) today = g_date_time_new_now_local();
    g_autofree gchar *today_date = g_date_time_format(today, "%Y%m%d");
    g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
    g_autofree gchar *dt_date = g_date_time_format(dt, "%Y%m%d");
    if (!strcmp(dt_date, today_date)) {
      gtk_calendar_select_day(GTK_CALENDAR(dc->calendar), tomorrow);
      gtk_calendar_select_day(GTK_CALENDAR(dc->calendar), today);
    } else {
      gtk_calendar_select_day(GTK_CALENDAR(dc->calendar), dt);
    }
  }
}

void errands_date_chooser_reset(ErrandsDateChooser *dc) {
  on_today_btn_clicked_cb(dc);
  g_object_set(dc->label, "label", _("Not Set"), NULL);
  g_object_set(dc->reset_btn, "visible", false, NULL);
}

// --- SIGNAL HANDLERS --- //

static void on_today_btn_clicked_cb(ErrandsDateChooser *dc) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  // We need to select tomorrow first so that calendar can select today (stupid)
  gtk_calendar_select_day(GTK_CALENDAR(dc->calendar), tomorrow);
  gtk_calendar_select_day(GTK_CALENDAR(dc->calendar), today);
}

static void on_tomorrow_btn_clicked_cb(ErrandsDateChooser *dc) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  gtk_calendar_select_day(GTK_CALENDAR(dc->calendar), tomorrow);
}

static void on_date_set_cb(ErrandsDateChooser *dc) {
  g_autoptr(GDateTime) date = gtk_calendar_get_date(GTK_CALENDAR(dc->calendar));
  g_autofree gchar *datetime = g_date_time_format(date, "%x");
  g_object_set(dc->label, "label", datetime, NULL);
  g_object_set(dc->reset_btn, "visible", true, NULL);
}
