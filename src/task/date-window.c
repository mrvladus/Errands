#include "date-window.h"
#include "../state.h"
#include "../utils.h"
#include "glib.h"
#include "gtk/gtk.h"

#include <glib/gi18n.h>

static void on_errands_date_window_close_cb(ErrandsDateWindow *win,
                                            gpointer data);
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

  // Start Date
  self->start_date_row = adw_action_row_new();
  g_object_set(self->start_date_row, "title", _("Date"), NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(start_group),
                            self->start_date_row);

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

static void on_start_date_set_cb(GtkCalendar *calendar,
                                 ErrandsDateWindow *win) {
  GDateTime *date = gtk_calendar_get_date(calendar);
  char *datetime = g_date_time_format(date, "%x");
  adw_action_row_set_subtitle(ADW_ACTION_ROW(win->start_date_row), datetime);
  g_free(datetime);
}
