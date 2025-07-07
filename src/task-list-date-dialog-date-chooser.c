#include "task-list-date-dialog-date-chooser.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_today_btn_clicked_cb(ErrandsTaskListDateDialogDateChooser *self);
static void on_tomorrow_btn_clicked_cb(ErrandsTaskListDateDialogDateChooser *self);
static void on_date_set_cb(ErrandsTaskListDateDialogDateChooser *self);

G_DEFINE_TYPE(ErrandsTaskListDateDialogDateChooser, errands_task_list_date_dialog_date_chooser, ADW_TYPE_ACTION_ROW)

static void errands_task_list_date_dialog_date_chooser_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER);
  G_OBJECT_CLASS(errands_task_list_date_dialog_date_chooser_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_date_chooser_class_init(ErrandsTaskListDateDialogDateChooserClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_date_chooser_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog-date-chooser.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogDateChooser, calendar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogDateChooser, reset_btn);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_today_btn_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_tomorrow_btn_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_date_set_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_date_dialog_date_chooser_reset);
}

static void errands_task_list_date_dialog_date_chooser_init(ErrandsTaskListDateDialogDateChooser *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialogDateChooser *errands_task_list_date_dialog_date_chooser_new() {
  return g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER, NULL);
}

icaltimetype errands_task_list_date_dialog_date_chooser_get_date(ErrandsTaskListDateDialogDateChooser *self) {
  g_autoptr(GDateTime) datetime = gtk_calendar_get_date(GTK_CALENDAR(self->calendar));
  g_autofree gchar *date_str = g_date_time_format(datetime, "%Y%m%d");
  return icaltime_from_string(date_str);
}

void errands_task_list_date_dialog_date_chooser_set_date(ErrandsTaskListDateDialogDateChooser *self,
                                                         icaltimetype date) {
  const char *date_str = icaltime_as_ical_string(date);
  LOG("Setting date to %s", date_str);
  // g_autoptr(GDateTime) dt;
  // if (!date.is_date) {
  //   char new_date[32];
  //   sprintf(new_date, "%sT000000Z", date_str);
  //   dt = g_date_time_new_from_iso8601(new_date, NULL);
  // } else dt = g_date_time_new_from_iso8601(date, NULL);
  // if (dt) {
  //   g_autoptr(GDateTime) today = g_date_time_new_now_local();
  //   g_autofree gchar *today_date = g_date_time_format(today, "%Y%m%d");
  //   g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  //   g_autofree gchar *dt_date = g_date_time_format(dt, "%Y%m%d");
  //   if (!strcmp(dt_date, today_date)) {
  //     gtk_calendar_select_day(GTK_CALENDAR(self->calendar), tomorrow);
  //     gtk_calendar_select_day(GTK_CALENDAR(self->calendar), today);
  //   } else {
  //     gtk_calendar_select_day(GTK_CALENDAR(self->calendar), dt);
  //   }
  // }
}

void errands_task_list_date_dialog_date_chooser_reset(ErrandsTaskListDateDialogDateChooser *self) {
  on_today_btn_clicked_cb(self);
  g_object_set(self, "subtitle", _("Not Set"), NULL);
  g_object_set(self->reset_btn, "visible", false, NULL);
}

// ---------- CALLBACKS ---------- //

static void on_today_btn_clicked_cb(ErrandsTaskListDateDialogDateChooser *self) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  // We need to select tomorrow first so that calendar can select today (stupid)
  gtk_calendar_select_day(GTK_CALENDAR(self->calendar), tomorrow);
  gtk_calendar_select_day(GTK_CALENDAR(self->calendar), today);
}

static void on_tomorrow_btn_clicked_cb(ErrandsTaskListDateDialogDateChooser *self) {
  g_autoptr(GDateTime) today = g_date_time_new_now_local();
  g_autoptr(GDateTime) tomorrow = g_date_time_add_days(today, 1);
  gtk_calendar_select_day(GTK_CALENDAR(self->calendar), tomorrow);
}

static void on_date_set_cb(ErrandsTaskListDateDialogDateChooser *self) {
  g_autoptr(GDateTime) date = gtk_calendar_get_date(GTK_CALENDAR(self->calendar));
  g_autofree gchar *datetime = g_date_time_format(date, "%x");
  g_object_set(self, "subtitle", datetime, NULL);
  g_object_set(self->reset_btn, "visible", true, NULL);
}
