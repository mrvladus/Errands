#include "../data/data.h"
#include "../state.h"
#include "../utils.h"
#include "task.h"

#include <glib/gi18n.h>

static void on_errands_date_window_close_cb(ErrandsDateWindow *win, gpointer data);
static void on_freq_changed_cb(AdwComboRow *row, GParamSpec *param, ErrandsDateWindow *win);
static void on_interval_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsDateWindow *win);
static void on_count_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsDateWindow *win);

G_DEFINE_TYPE(ErrandsDateWindow, errands_date_window, ADW_TYPE_DIALOG)

static void errands_date_window_class_init(ErrandsDateWindowClass *class) {}

static void errands_date_window_init(ErrandsDateWindow *self) {
  LOG("Date Widdow: Create");

  g_object_set(self, "title", _("Date and Time"), "content-width", 360, "content-height", 460, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_date_window_close_cb), NULL);

  GtkWidget *list_box = gtk_list_box_new();
  g_object_set(list_box, "selection-mode", GTK_SELECTION_NONE, "valign", GTK_ALIGN_START, "margin-start", 12,
               "margin-end", 12, "margin-bottom", 24, "margin-top", 12, NULL);
  gtk_widget_add_css_class(list_box, "boxed-list");

  // Start Date
  self->start_date_row = adw_action_row_new();
  g_object_set(self->start_date_row, "title", _("Start Date"), NULL);
  gtk_list_box_append(GTK_LIST_BOX(list_box), self->start_date_row);

  self->start_date_chooser = errands_date_chooser_new();
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_date_row), GTK_WIDGET(self->start_date_chooser));

  // Start Time
  self->start_time_row = adw_action_row_new();
  g_object_set(self->start_time_row, "title", _("Start Time"), NULL);
  gtk_list_box_append(GTK_LIST_BOX(list_box), self->start_time_row);

  self->start_time_chooser = errands_time_chooser_new();
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_time_row), GTK_WIDGET(self->start_time_chooser));

  // --- Repeat  --- //

  self->repeat_row = adw_expander_row_new();
  g_object_set(self->repeat_row, "title", _("Repeat"), "show-enable-switch", true, NULL);
  gtk_list_box_append(GTK_LIST_BOX(list_box), self->repeat_row);

  // Frequency
  GtkStringList *freq_model = gtk_string_list_new(
      (const char *const[]){_("Minutely"), _("Hourly"), _("Daily"), _("Weekly"), _("Monthly"), _("Yearly"), NULL});
  self->frequency_row = adw_combo_row_new();
  g_object_set(self->frequency_row, "title", _("Frequency"), "subtitle", _("How often task will repeat"), "model",
               freq_model, NULL);
  g_signal_connect(self->frequency_row, "notify::selected", G_CALLBACK(on_freq_changed_cb), self);
  adw_expander_row_add_row(ADW_EXPANDER_ROW(self->repeat_row), self->frequency_row);

  // Interval
  self->interval_row = adw_spin_row_new_with_range(1, 365, 1);
  g_object_set(self->interval_row, "title", _("Interval"), NULL);
  g_signal_connect(self->interval_row, "notify::value", G_CALLBACK(on_interval_changed_cb), self);
  adw_expander_row_add_row(ADW_EXPANDER_ROW(self->repeat_row), self->interval_row);

  // Week chooser
  self->week_chooser = errands_week_chooser_new();
  adw_expander_row_add_row(ADW_EXPANDER_ROW(self->repeat_row), GTK_WIDGET(self->week_chooser));

  // Month chooser
  self->month_chooser = errands_month_chooser_new();
  adw_expander_row_add_row(ADW_EXPANDER_ROW(self->repeat_row), GTK_WIDGET(self->month_chooser));

  // Until Date
  self->until_row = adw_action_row_new();
  g_object_set(self->until_row, "title", _("Until"), NULL);
  self->until_date_chooser = errands_date_chooser_new();
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->until_row), GTK_WIDGET(self->until_date_chooser));
  adw_expander_row_add_row(ADW_EXPANDER_ROW(self->repeat_row), self->until_row);

  // Count
  self->count_row = adw_spin_row_new_with_range(0, 100, 1);
  g_object_set(self->count_row, "title", _("Count"), NULL);
  g_signal_connect(self->count_row, "notify::value", G_CALLBACK(on_count_changed_cb), self);
  adw_expander_row_add_row(ADW_EXPANDER_ROW(self->repeat_row), self->count_row);

  // --- Due --- //

  // Due date
  self->due_date_row = adw_action_row_new();
  g_object_set(self->due_date_row, "title", _("Due Date"), NULL);
  g_object_bind_property(self->repeat_row, "enable-expansion", self->due_date_row, "visible",
                         G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_list_box_append(GTK_LIST_BOX(list_box), self->due_date_row);

  self->due_date_chooser = errands_date_chooser_new();
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->due_date_row), GTK_WIDGET(self->due_date_chooser));

  // Due time
  self->due_time_row = adw_action_row_new();
  g_object_set(self->due_time_row, "title", _("Due Time"), NULL);
  g_object_bind_property(self->repeat_row, "enable-expansion", self->due_time_row, "visible",
                         G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_list_box_append(GTK_LIST_BOX(list_box), self->due_time_row);

  self->due_time_chooser = errands_time_chooser_new();
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->due_time_row), GTK_WIDGET(self->due_time_chooser));

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  g_object_set(scrl, "child", list_box, NULL);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "content", scrl, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), adw_header_bar_new());
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsDateWindow *errands_date_window_new() { return g_object_ref_sink(g_object_new(ERRANDS_TYPE_DATE_WINDOW, NULL)); }

void errands_date_window_show(ErrandsTask *task) {
  if (!state.date_window) state.date_window = errands_date_window_new();

  TaskData *data = task->data;

  LOG("Date window: Show for '%s'", errands_data_get_str(data, DATA_PROP_UID));

  state.date_window->task = task;

  // Reset all rows
  errands_date_chooser_reset(state.date_window->start_date_chooser);
  errands_time_chooser_reset(state.date_window->start_time_chooser);
  errands_date_chooser_reset(state.date_window->due_date_chooser);
  errands_time_chooser_reset(state.date_window->due_time_chooser);
  adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 2);
  adw_spin_row_set_value(ADW_SPIN_ROW(state.date_window->interval_row), 1);
  errands_week_chooser_reset(state.date_window->week_chooser);
  errands_month_chooser_reset(state.date_window->month_chooser);
  errands_date_chooser_reset(state.date_window->until_date_chooser);

  // Set start date and time

  // Set start date
  const char *start_dt = errands_data_get_str(data, DATA_PROP_START);
  if (start_dt) {
    g_autoptr(GString) s_date = g_string_new(start_dt);
    // If start date not empty
    if (strcmp(s_date->str, "")) {
      errands_date_chooser_set_date(state.date_window->start_date_chooser, s_date->str);
      LOG("Date window: Set start date to '%s'", s_date->str);
      // Set start time
      const char *s_time = strstr(s_date->str, "T");
      if (s_time) {
        s_time++;
        errands_time_chooser_set_time(state.date_window->start_time_chooser, s_time);
        LOG("Date window: Set start time to '%s'", s_time);
      }
    }
  }

  // Setup due date and time
  const char *due_dt = errands_data_get_str(data, DATA_PROP_DUE);
  if (due_dt) {
    g_autoptr(GString) d_date = g_string_new(errands_data_get_str(data, DATA_PROP_DUE));
    // If due date not empty set date
    if (strcmp(d_date->str, "")) {
      errands_date_chooser_set_date(state.date_window->due_date_chooser, d_date->str);
      LOG("Date window: Set due date to '%s'", d_date->str);
      // If time is set
      const char *d_time = strstr(d_date->str, "T");
      if (d_time) {
        d_time++;
        errands_time_chooser_set_time(state.date_window->due_time_chooser, d_time);
        LOG("Date window: Set due time to '%s'", d_time);
      }
    }
  }

  // Setup repeat
  const char *rrule = errands_data_get_str(data, DATA_PROP_RRULE);
  bool is_repeated = rrule ? true : false;
  adw_expander_row_set_enable_expansion(ADW_EXPANDER_ROW(state.date_window->repeat_row), is_repeated);
  adw_expander_row_set_expanded(ADW_EXPANDER_ROW(state.date_window->repeat_row), is_repeated);
  // if (!is_repeated) {
  //   LOG("Date window: Reccurrence is disabled");
  // } else {
  //   // Set frequency
  //   char *freq = get_rrule_value(rrule, "FREQ");
  //   if (freq) {
  //     if (!strcmp(freq, "MINUTELY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 0);
  //     else if (!strcmp(freq, "HOURLY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 1);
  //     else if (!strcmp(freq, "DAILY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 2);
  //     else if (!strcmp(freq, "WEEKLY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 3);
  //     else if (!strcmp(freq, "MONTHLY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 4);
  //     else if (!strcmp(freq, "YEARLY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 5);
  //     LOG("Date window: Set frequency to '%s'", freq);
  //     free(freq);
  //   }

  //   // Set interval
  //   char *interval_str = get_rrule_value(rrule, "INTERVAL");
  //   if (interval_str) {
  //     if (string_is_number(interval_str)) {
  //       int interval = atoi(interval_str);
  //       adw_spin_row_set_value(ADW_SPIN_ROW(state.date_window->interval_row), interval);
  //     }
  //     LOG("Date window: Set interval to '%s'", interval_str);
  //     free(interval_str);
  //   } else {
  //     adw_spin_row_set_value(ADW_SPIN_ROW(state.date_window->interval_row), 1);
  //     LOG("Date window: Set interval to '1'");
  //   }

  //   // Set weekdays
  //   errands_week_chooser_set_days(state.date_window->week_chooser, rrule);

  //   // Set month
  //   errands_month_chooser_set_months(state.date_window->month_chooser, rrule);

  //   // Set until date
  //   char *until = get_rrule_value(rrule, "UNTIL");
  //   if (until) {
  //     errands_date_chooser_set_date(state.date_window->until_date_chooser, until);
  //     LOG("Date window: Set until date to '%s'", until);
  //     free(until);
  //   } else {
  //     // Set count
  //     char *count_str = get_rrule_value(rrule, "COUNT");
  //     if (count_str) {
  //       if (string_is_number(count_str)) {
  //         int count = atoi(count_str);
  //         adw_spin_row_set_value(ADW_SPIN_ROW(state.date_window->count_row), count);
  //       }
  //       LOG("Date window: Set count to '%s'", count_str);
  //       free(count_str);
  //     } else {
  //       adw_spin_row_set_value(ADW_SPIN_ROW(state.date_window->count_row), 0);
  //       LOG("Date window: Set count to infinite");
  //     }
  //   }
  // }

  adw_dialog_present(ADW_DIALOG(state.date_window), GTK_WIDGET(state.main_window));
}

// --- SIGNAL HANDLERS --- //

static void on_errands_date_window_close_cb(ErrandsDateWindow *win, gpointer _data) {
  TaskData *data = win->task->data;

  // Set start datetime
  bool start_date_is_set =
      !strcmp(gtk_label_get_label(GTK_LABEL(win->start_date_chooser->label)), _("Not Set")) ? false : true;
  bool start_time_is_set =
      !strcmp(gtk_label_get_label(GTK_LABEL(win->start_time_chooser->label)), _("Not Set")) ? false : true;
  LOG("%d %d", start_date_is_set, start_time_is_set);
  g_autoptr(GString) s_dt = g_string_new("");
  if (start_date_is_set) {
    const char *start_date = errands_date_chooser_get_date(win->start_date_chooser);
    g_string_append(s_dt, start_date);
    if (start_time_is_set) {
      const char *start_time = errands_time_chooser_get_time(win->start_time_chooser);
      g_string_append_printf(s_dt, "T%sZ", start_time);
    }
    errands_data_set_str(data, DATA_PROP_START, s_dt->str);
  } else {
    if (start_time_is_set) {
      const char *start_time = errands_time_chooser_get_time(win->start_time_chooser);
      char *today = get_today_date();
      g_string_append_printf(s_dt, "%sT%sZ", today, start_time);
      free(today);
    }
  }
  errands_data_set_str(data, DATA_PROP_START, s_dt->str);

  // Set due datetime
  bool due_time_is_set =
      !strcmp(gtk_label_get_label(GTK_LABEL(win->due_time_chooser->label)), _("Not Set")) ? false : true;
  bool due_date_is_set =
      !strcmp(gtk_label_get_label(GTK_LABEL(win->due_date_chooser->label)), _("Not Set")) ? false : true;
  g_autoptr(GString) d_dt = g_string_new("");
  if (due_date_is_set) {
    const char *due_date = errands_date_chooser_get_date(win->due_date_chooser);
    g_string_append(d_dt, due_date);
    if (due_time_is_set) {
      const char *due_time = errands_time_chooser_get_time(win->due_time_chooser);
      g_string_append_printf(d_dt, "T%sZ", due_time);
    }
    errands_data_set_str(data, DATA_PROP_DUE, d_dt->str);
  } else {
    if (due_time_is_set) {
      const char *due_time = errands_time_chooser_get_time(win->due_time_chooser);
      g_string_append_printf(d_dt, "%sT%sZ", get_today_date(), due_time);
    }
  }
  errands_data_set_str(data, DATA_PROP_DUE, d_dt->str);

  // Check if repeat is enabled
  if (!adw_expander_row_get_enable_expansion(ADW_EXPANDER_ROW(win->repeat_row)))
    // If not - clean rrule
    errands_data_set_str(data, DATA_PROP_RRULE, "");
  // Generate new rrule
  // else {
  //   // Get frequency
  //   guint frequency = adw_combo_row_get_selected(ADW_COMBO_ROW(win->frequency_row));
  //   LOG("HERE");
  //   // Create new rrule string
  //   g_autoptr(GString) rrule = g_string_new("RRULE:");

  //   // Set frequency
  //   if (frequency == 0)
  //     g_string_append(rrule, "FREQ=MINUTELY;");
  //   else if (frequency == 1)
  //     g_string_append(rrule, "FREQ=HOURLY;");
  //   else if (frequency == 2)
  //     g_string_append(rrule, "FREQ=DAILY;");
  //   else if (frequency == 3)
  //     g_string_append(rrule, "FREQ=WEEKLY;");
  //   else if (frequency == 4)
  //     g_string_append(rrule, "FREQ=MONTHLY;");
  //   else if (frequency == 5)
  //     g_string_append(rrule, "FREQ=YEARLY;");

  //   // Set interval
  //   // g_string_append_printf(rrule, "INTERVAL=%d;", (int)adw_spin_row_get_value(ADW_SPIN_ROW(win->interval_row)));

  //   // // Set week days
  //   // g_string_append(rrule, errands_week_chooser_get_days(win->week_chooser));

  //   // // Set months
  //   // g_string_append(rrule, errands_month_chooser_get_months(win->month_chooser));

  //   // // Set UNTIL if until date is set
  //   // const char *until_label = gtk_label_get_label(GTK_LABEL(win->until_date_chooser->label));
  //   // if (strcmp(until_label, _("Not Set"))) {
  //   //   g_autoptr(GDateTime) until_d = gtk_calendar_get_date(GTK_CALENDAR(win->until_date_chooser->calendar));
  //   //   g_autofree gchar *until_d_str = g_date_time_format(until_d, "%Y%m%d");
  //   //   g_string_append_printf(rrule, "UNTIL=%s", until_d_str);
  //   //   // If start date contains time - add it to the until date
  //   //   const char *time = strstr(task_data_get_start(data), "T");
  //   //   if (time)
  //   //     g_string_append(rrule, time);
  //   //   g_string_append(rrule, ";");
  //   // }

  //   // // Set count if until is not set
  //   // int count = adw_spin_row_get_value(ADW_SPIN_ROW(win->count_row));
  //   // if (count > 0 && !strstr(rrule->str, "UNTIL="))
  //   //   g_string_append_printf(rrule, "COUNT=%d;", count);

  //   // Save rrule
  //   if (strcmp(rrule->str, "RRULE:"))
  //     task_data_set_rrule(data, rrule->str);
  // }
  errands_data_write_list(task_data_get_list(data));
  // Set date button text
  errands_task_toolbar_update_date_btn(win->task->toolbar);
  // TODO: sync
}

static void on_freq_changed_cb(AdwComboRow *row, GParamSpec *param, ErrandsDateWindow *win) {
  int idx = adw_combo_row_get_selected(row);
  gtk_widget_set_visible(GTK_WIDGET(win->week_chooser), idx > 2);
  // Update interval row
  on_interval_changed_cb(ADW_SPIN_ROW(win->interval_row), NULL, win);
  on_count_changed_cb(ADW_SPIN_ROW(win->count_row), NULL, win);
}

// Set interval row subtitle
static void on_interval_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsDateWindow *win) {
  // Get frequency
  int selected_freq = adw_combo_row_get_selected(ADW_COMBO_ROW(win->frequency_row));

  char *ending;
  if (selected_freq == 0) ending = (char *)C_("Every ...", "minutes");
  else if (selected_freq == 1) ending = (char *)C_("Every ...", "hours");
  else if (selected_freq == 2) ending = (char *)C_("Every ...", "days");
  else if (selected_freq == 3) ending = (char *)C_("Every ...", "weeks");
  else if (selected_freq == 4) ending = (char *)C_("Every ...", "months");
  else if (selected_freq == 5) ending = (char *)C_("Every ...", "years");

  // Set subtitle
  g_autofree gchar *subtitle = g_strdup_printf(_("Repeat every %d %s"), (int)adw_spin_row_get_value(row), ending);
  g_object_set(row, "subtitle", subtitle, NULL);
}

static void on_count_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsDateWindow *win) {
  const size_t value = adw_spin_row_get_value(row);
  if (value == 0) g_object_set(row, "subtitle", _("Repeat forever"), NULL);
  else {
    g_autofree gchar *subtitle = g_strdup_printf(_("Repeat %zu times"), value);
    g_object_set(row, "subtitle", subtitle, NULL);
  }
}
