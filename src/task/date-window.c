#include "date-window.h"
#include "../state.h"
#include "../utils.h"
#include "adwaita.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"

#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_errands_date_window_close_cb(ErrandsDateWindow *win,
                                            gpointer data);
static void on_time_set_cb(ErrandsTimeChooser *tc, GtkLabel *label);
static void on_start_date_set_cb(GtkCalendar *calendar, ErrandsDateWindow *win);
static void on_due_time_set_cb(ErrandsTimeChooser *tc, ErrandsDateWindow *win);
static void on_due_date_set_cb(GtkCalendar *calendar, ErrandsDateWindow *win);

G_DEFINE_TYPE(ErrandsDateWindow, errands_date_window, ADW_TYPE_DIALOG)

static void errands_date_window_class_init(ErrandsDateWindowClass *class) {}

static void errands_date_window_init(ErrandsDateWindow *self) {
  LOG("Creating date window");

  g_object_set(self, "title", _("Date and Time"), "content-width", 360,
               "content-height", 600, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_date_window_close_cb),
                   NULL);

  // --- Start group --- //
  GtkWidget *start_group = adw_preferences_group_new();
  g_object_set(start_group, "title", _("Start"), NULL);

  // Start Time
  self->start_time_row = adw_action_row_new();
  g_object_set(self->start_time_row, "title", _("Time"), NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(start_group),
                            self->start_time_row);

  self->start_time_label = gtk_label_new("");
  gtk_widget_add_css_class(self->start_time_label, "numeric");
  gtk_widget_add_css_class(self->start_time_label, "heading");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_time_row),
                            self->start_time_label);

  self->start_time_chooser = errands_time_chooser_new();
  g_signal_connect(self->start_time_chooser, "changed",
                   G_CALLBACK(on_time_set_cb), self->start_time_label);

  GtkWidget *start_time_btn_popover = gtk_popover_new();
  g_object_set(start_time_btn_popover, "child", self->start_time_chooser, NULL);

  GtkWidget *start_time_btn = gtk_menu_button_new();
  g_object_set(start_time_btn, "popover", start_time_btn_popover, "icon-name",
               "errands-clock-symbolic", "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(start_time_btn, "flat");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_time_row),
                            start_time_btn);

  // Start Date
  // self->start_date_row = adw_action_row_new();
  // g_object_set(self->start_date_row, "title", _("Date"), NULL);
  // adw_preferences_group_add(ADW_PREFERENCES_GROUP(start_group),
  //                           self->start_date_row);

  // self->start_date_label = gtk_label_new("");
  // gtk_widget_add_css_class(self->start_date_label, "numeric");
  // gtk_widget_add_css_class(self->start_date_label, "dim-label");
  // adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_date_row),
  //                           self->start_date_label);

  // self->start_calendar = gtk_calendar_new();
  // g_signal_connect(self->start_calendar, "day-selected",
  //                  G_CALLBACK(on_start_date_set_cb), self);

  // GtkWidget *start_calendar_button_popover = gtk_popover_new();
  // g_object_set(start_calendar_button_popover, "child", self->start_calendar,
  //              NULL);

  // GtkWidget *start_calendar_button = gtk_menu_button_new();
  // g_object_set(start_calendar_button, "popover",
  // start_calendar_button_popover,
  //              "icon-name", "errands-calendar-symbolic", "valign",
  //              GTK_ALIGN_CENTER, NULL);
  // gtk_widget_add_css_class(start_calendar_button, "flat");
  // adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_date_row),
  //                           start_calendar_button);

  self->start_date_all_day_row = adw_switch_row_new();
  g_object_set(self->start_date_all_day_row, "title", _("All Day"), NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(start_group),
                            self->start_date_all_day_row);

  // --- Due group --- //

  GtkWidget *due_group = adw_preferences_group_new();
  g_object_set(due_group, "title", _("Due"), NULL);
  g_object_bind_property(self->start_date_all_day_row, "active", due_group,
                         "visible",
                         G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  self->due_time_row = adw_action_row_new();
  g_object_set(self->due_time_row, "title", _("Time"), NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(due_group),
                            self->due_time_row);

  self->due_time_label = gtk_label_new("");
  gtk_widget_add_css_class(self->due_time_label, "numeric");
  gtk_widget_add_css_class(self->due_time_label, "heading");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->due_time_row),
                            self->due_time_label);

  self->due_time_chooser = errands_time_chooser_new();
  g_signal_connect(self->due_time_chooser, "changed",
                   G_CALLBACK(on_time_set_cb), self->due_time_label);

  GtkWidget *due_time_btn_popover = gtk_popover_new();
  g_object_set(due_time_btn_popover, "child", self->due_time_chooser, NULL);

  GtkWidget *due_time_btn = gtk_menu_button_new();
  g_object_set(due_time_btn, "popover", due_time_btn_popover, "icon-name",
               "errands-clock-symbolic", "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(due_time_btn, "flat");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->due_time_row), due_time_btn);

  // // Due Date
  // self->due_date_row = adw_action_row_new();
  // g_object_set(self->due_date_row, "title", _("Date"), NULL);
  // adw_preferences_group_add(ADW_PREFERENCES_GROUP(due_group),
  //                           self->due_date_row);

  // self->due_date_label = gtk_label_new("");
  // gtk_widget_add_css_class(self->due_date_label, "numeric");
  // gtk_widget_add_css_class(self->due_date_label, "dim-label");
  // adw_action_row_add_suffix(ADW_ACTION_ROW(self->due_date_row),
  //                           self->due_date_label);

  // self->due_calendar = gtk_calendar_new();
  // g_signal_connect(self->due_calendar, "day-selected",
  //                  G_CALLBACK(on_due_date_set_cb), self);

  // GtkWidget *due_calendar_button_popover = gtk_popover_new();
  // g_object_set(due_calendar_button_popover, "child", self->due_calendar,
  // NULL);

  // GtkWidget *due_calendar_button = gtk_menu_button_new();
  // g_object_set(due_calendar_button, "popover", due_calendar_button_popover,
  //              "icon-name", "errands-calendar-symbolic", "valign",
  //              GTK_ALIGN_CENTER, NULL);
  // gtk_widget_add_css_class(due_calendar_button, "flat");
  // adw_action_row_add_suffix(ADW_ACTION_ROW(self->due_date_row),
  //                           due_calendar_button);

  // // --- Repeat group --- //

  GtkWidget *repeat_group = adw_preferences_group_new();
  g_object_set(repeat_group, "title", _("Repeat"), NULL);

  // --- Page --- //

  GtkWidget *page = adw_preferences_page_new();
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                           ADW_PREFERENCES_GROUP(start_group));
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                           ADW_PREFERENCES_GROUP(due_group));
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                           ADW_PREFERENCES_GROUP(repeat_group));

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "content", page, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), adw_header_bar_new());
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsDateWindow *errands_date_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_DATE_WINDOW, NULL));
}

void errands_date_window_show(ErrandsTask *task) {
  state.date_window->task = task;
  char *s_dt = task->data->start_date;
  // If start date not empty
  if (strcmp(s_dt, "")) {

    // If time is set
    if (string_contains(s_dt, "T")) {
    }
  }
  adw_dialog_present(ADW_DIALOG(state.date_window),
                     GTK_WIDGET(state.main_window));
}

// --- SIGNAL HANDLERS --- //

static void on_errands_date_window_close_cb(ErrandsDateWindow *win,
                                            gpointer data) {}

static void on_time_set_cb(ErrandsTimeChooser *tc, GtkLabel *label) {
  char time[8];
  sprintf(time, "%c%c:%c%c", tc->time->str[0], tc->time->str[1],
          tc->time->str[2], tc->time->str[3]);
  gtk_label_set_label(label, time);
}

// static void on_start_time_preset_cb(const char *h) {}

// static void on_start_date_set_cb(GtkCalendar *calendar,
//                                  ErrandsDateWindow *win) {
//   GDateTime *date = gtk_calendar_get_date(calendar);
//   char *datetime = g_date_time_format(date, "%x");
//   g_object_set(win->start_date_label, "label", datetime, NULL);
//   g_free(datetime);
// }

// static void on_due_time_set_cb(ErrandsDateWindow *win) {
//   gchar *h = g_strdup_printf(
//       "%d",
//       gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(win->due_time_h)));
//   gtk_editable_set_text(GTK_EDITABLE(win->due_time_h), h);
//   gchar *m = g_strdup_printf("%02d", gtk_spin_button_get_value_as_int(
//                                          GTK_SPIN_BUTTON(win->due_time_m)));
//   gtk_editable_set_text(GTK_EDITABLE(win->due_time_m), m);
//   gchar *hm = g_strdup_printf("%s:%s", h, m);
//   g_object_set(win->due_time_label, "label", hm, NULL);
//   g_signal_emit_by_name(win->due_calendar, "day-selected", NULL);

//   g_free(h);
//   g_free(m);
//   g_free(hm);
// }

// static void on_due_time_preset_cb(const char *h) {
//   gtk_spin_button_set_value(GTK_SPIN_BUTTON(state.date_window->due_time_m),
//   0);
//   gtk_spin_button_set_value(GTK_SPIN_BUTTON(state.date_window->due_time_h),
//                             atoi(h));
// }

// static void on_due_date_set_cb(GtkCalendar *calendar, ErrandsDateWindow *win)
// {
//   GDateTime *date = gtk_calendar_get_date(calendar);
//   char *datetime = g_date_time_format(date, "%x");
//   g_object_set(win->due_date_label, "label", datetime, NULL);
//   g_free(datetime);
// }
