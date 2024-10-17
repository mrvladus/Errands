#include "date-window.h"
#include "../state.h"
#include "../utils.h"
#include "adwaita.h"
#include "glib.h"
#include "gtk/gtk.h"

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

static void on_errands_date_window_close_cb(ErrandsDateWindow *win,
                                            gpointer data);
static void on_start_time_set_cb(ErrandsDateWindow *win);
static void on_start_time_preset_cb(const char *time);
static void on_start_date_set_cb(GtkCalendar *calendar, ErrandsDateWindow *win);

G_DEFINE_TYPE(ErrandsDateWindow, errands_date_window, ADW_TYPE_DIALOG)

static void errands_date_window_class_init(ErrandsDateWindowClass *class) {}

static void errands_date_window_init(ErrandsDateWindow *self) {
  LOG("Creating date window");

  g_object_set(self, "title", _("Date and Time"), "content-width", 360,
               "content-height", 400, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_date_window_close_cb),
                   NULL);

  // Start group
  GtkWidget *start_group = adw_preferences_group_new();
  g_object_set(start_group, "title", _("Start"), NULL);

  // Start Time
  self->start_time_row = adw_action_row_new();
  g_object_set(self->start_time_row, "title", _("Time"), NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(start_group),
                            self->start_time_row);

  self->start_time_label = gtk_label_new("");
  gtk_widget_add_css_class(self->start_time_label, "numeric");
  gtk_widget_add_css_class(self->start_time_label, "dim-label");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_time_row),
                            self->start_time_label);

  // Start time popover menu

  GtkWidget *morning_btn = gtk_button_new();
  GtkWidget *morning_btn_content = adw_button_content_new();
  g_object_set(morning_btn_content, "icon-name", "errands-morning-symbolic",
               "label", "9:00", NULL);
  g_object_set(morning_btn, "child", morning_btn_content, NULL);
  g_signal_connect_swapped(morning_btn, "clicked",
                           G_CALLBACK(on_start_time_preset_cb), "9");

  GtkWidget *afternoon_btn = gtk_button_new();
  GtkWidget *afternoon_btn_content = adw_button_content_new();
  g_object_set(afternoon_btn_content, "icon-name", "errands-afternoon-symbolic",
               "label", "12:00", NULL);
  g_object_set(afternoon_btn, "child", afternoon_btn_content, NULL);
  g_signal_connect_swapped(afternoon_btn, "clicked",
                           G_CALLBACK(on_start_time_preset_cb), "12");

  GtkWidget *sunset_btn = gtk_button_new();
  GtkWidget *sunset_btn_content = adw_button_content_new();
  g_object_set(sunset_btn_content, "icon-name", "errands-sunset-symbolic",
               "label", "17:00", NULL);
  g_object_set(sunset_btn, "child", sunset_btn_content, NULL);
  g_signal_connect_swapped(sunset_btn, "clicked",
                           G_CALLBACK(on_start_time_preset_cb), "17");

  GtkWidget *night_btn = gtk_button_new();
  GtkWidget *night_btn_content = adw_button_content_new();
  g_object_set(night_btn_content, "icon-name", "errands-night-symbolic",
               "label", "20:00", NULL);
  g_object_set(night_btn, "child", night_btn_content, NULL);
  g_signal_connect_swapped(night_btn, "clicked",
                           G_CALLBACK(on_start_time_preset_cb), "20");

  GtkWidget *clock_preset_vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  g_object_set(clock_preset_vbox1, "homogeneous", true, NULL);
  gtk_box_append(GTK_BOX(clock_preset_vbox1), morning_btn);
  gtk_box_append(GTK_BOX(clock_preset_vbox1), afternoon_btn);
  GtkWidget *clock_preset_vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  g_object_set(clock_preset_vbox2, "homogeneous", true, NULL);
  gtk_box_append(GTK_BOX(clock_preset_vbox2), sunset_btn);
  gtk_box_append(GTK_BOX(clock_preset_vbox2), night_btn);

  self->start_time_h = gtk_spin_button_new_with_range(0, 23, 1);
  g_object_set(self->start_time_h, "orientation", GTK_ORIENTATION_VERTICAL,
               NULL);
  g_signal_connect_swapped(self->start_time_h, "value-changed",
                           G_CALLBACK(on_start_time_set_cb), self);

  self->start_time_m = gtk_spin_button_new_with_range(0, 59, 1);
  g_object_set(self->start_time_m, "orientation", GTK_ORIENTATION_VERTICAL,
               NULL);
  g_signal_connect_swapped(self->start_time_m, "value-changed",
                           G_CALLBACK(on_start_time_set_cb), self);

  GtkWidget *clock_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append(GTK_BOX(clock_hbox), self->start_time_h);
  gtk_box_append(GTK_BOX(clock_hbox), gtk_label_new(":"));
  gtk_box_append(GTK_BOX(clock_hbox), self->start_time_m);
  gtk_box_append(GTK_BOX(clock_hbox), clock_preset_vbox1);
  gtk_box_append(GTK_BOX(clock_hbox), clock_preset_vbox2);

  GtkWidget *start_time_btn_popover = gtk_popover_new();
  g_object_set(start_time_btn_popover, "child", clock_hbox, NULL);
  g_signal_connect_swapped(start_time_btn_popover, "show",
                           G_CALLBACK(on_start_time_set_cb), self);

  GtkWidget *start_time_btn = gtk_menu_button_new();
  g_object_set(start_time_btn, "popover", start_time_btn_popover, "icon-name",
               "errands-clock-symbolic", "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(start_time_btn, "flat");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_time_row),
                            start_time_btn);

  // Start Date
  self->start_date_row = adw_action_row_new();
  g_object_set(self->start_date_row, "title", _("Date"), NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(start_group),
                            self->start_date_row);

  self->start_date_label = gtk_label_new("");
  gtk_widget_add_css_class(self->start_date_label, "numeric");
  gtk_widget_add_css_class(self->start_date_label, "dim-label");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_date_row),
                            self->start_date_label);

  self->start_calendar = gtk_calendar_new();
  g_signal_connect(self->start_calendar, "day-selected",
                   G_CALLBACK(on_start_date_set_cb), self);

  GtkWidget *start_calendar_button_popover = gtk_popover_new();
  g_object_set(start_calendar_button_popover, "child", self->start_calendar,
               NULL);

  GtkWidget *start_calendar_button = gtk_menu_button_new();
  g_object_set(start_calendar_button, "popover", start_calendar_button_popover,
               "icon-name", "errands-calendar-symbolic", "valign",
               GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(start_calendar_button, "flat");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_date_row),
                            start_calendar_button);

  GtkWidget *page = adw_preferences_page_new();
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page),
                           ADW_PREFERENCES_GROUP(start_group));

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
  adw_dialog_present(ADW_DIALOG(state.date_window),
                     GTK_WIDGET(state.main_window));
}

// --- SIGNAL HANDLERS --- //

static void on_errands_date_window_close_cb(ErrandsDateWindow *win,
                                            gpointer data) {}

static void on_start_time_set_cb(ErrandsDateWindow *win) {
  gchar *h = g_strdup_printf("%d", gtk_spin_button_get_value_as_int(
                                       GTK_SPIN_BUTTON(win->start_time_h)));
  gtk_editable_set_text(GTK_EDITABLE(win->start_time_h), h);
  gchar *m = g_strdup_printf("%02d", gtk_spin_button_get_value_as_int(
                                         GTK_SPIN_BUTTON(win->start_time_m)));
  gtk_editable_set_text(GTK_EDITABLE(win->start_time_m), m);
  gchar *hm = g_strdup_printf("%s:%s", h, m);
  g_object_set(win->start_time_label, "label", hm, NULL);
  g_signal_emit_by_name(win->start_calendar, "day-selected", NULL);

  g_free(h);
  g_free(m);
  g_free(hm);
}

static void on_start_date_set_cb(GtkCalendar *calendar,
                                 ErrandsDateWindow *win) {
  GDateTime *date = gtk_calendar_get_date(calendar);
  char *datetime = g_date_time_format(date, "%x");
  g_object_set(win->start_date_label, "label", datetime, NULL);
  g_free(datetime);
}

static void on_start_time_preset_cb(const char *h) {
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(state.date_window->start_time_m),
                            0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(state.date_window->start_time_h),
                            atoi(h));
}
