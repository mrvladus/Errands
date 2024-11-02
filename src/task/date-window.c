#include "date-window.h"
#include "../state.h"
#include "../utils.h"

#include <glib/gi18n.h>

static void on_errands_date_window_close_cb(ErrandsDateWindow *win, gpointer data);
static void on_freq_changed_cb(AdwComboRow *row, GParamSpec *param, ErrandsDateWindow *win);
static void on_interval_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsDateWindow *win);
static void on_count_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsDateWindow *win);

G_DEFINE_TYPE(ErrandsDateWindow, errands_date_window, ADW_TYPE_DIALOG)

static void errands_date_window_class_init(ErrandsDateWindowClass *class) {}

static void errands_date_window_init(ErrandsDateWindow *self) {
  LOG("Creating date window");

  g_object_set(self, "title", _("Date and Time"), "content-width", 360, "content-height", 460,
               NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_date_window_close_cb), NULL);

  GtkWidget *list_box = gtk_list_box_new();
  g_object_set(list_box, "selection-mode", GTK_SELECTION_NONE, "valign", GTK_ALIGN_START,
               "margin-start", 12, "margin-end", 12, "margin-bottom", 24, "margin-top", 12, NULL);
  gtk_widget_add_css_class(list_box, "boxed-list");

  // Start Date
  self->start_date_row = adw_action_row_new();
  g_object_set(self->start_date_row, "title", _("Start Date"), NULL);
  gtk_list_box_append(GTK_LIST_BOX(list_box), self->start_date_row);

  self->start_date_chooser = errands_date_chooser_new();
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_date_row),
                            GTK_WIDGET(self->start_date_chooser));

  // Start Time
  self->start_time_row = adw_action_row_new();
  g_object_set(self->start_time_row, "title", _("Start Time"), NULL);
  gtk_list_box_append(GTK_LIST_BOX(list_box), self->start_time_row);

  self->start_time_chooser = errands_time_chooser_new();
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->start_time_row),
                            GTK_WIDGET(self->start_time_chooser));

  // --- Repeat  --- //

  self->repeat_row = adw_expander_row_new();
  g_object_set(self->repeat_row, "title", _("Repeat"), "show-enable-switch", true, NULL);
  gtk_list_box_append(GTK_LIST_BOX(list_box), self->repeat_row);

  // Frequency
  const char *const freq_items[] = {_("Minutely"), _("Hourly"), _("Daily"), _("Weekly"),
                                    _("Monthly"),  _("Yearly"), NULL};
  GtkStringList *freq_model = gtk_string_list_new(freq_items);
  self->frequency_row = adw_combo_row_new();
  g_object_set(self->frequency_row, "title", _("Frequency"), "subtitle",
               _("How often task will repeat"), "model", freq_model, NULL);
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

  // Month
  GtkWidget *month_row_box =
      g_object_new(GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "spacing", 8,
                   "margin-start", 12, "margin-end", 12, "margin-top", 6, "margin-bottom", 6, NULL);
  gtk_box_append(GTK_BOX(month_row_box), g_object_new(GTK_TYPE_LABEL, "label", _("Months"),
                                                      "halign", GTK_ALIGN_START, NULL));
  GtkWidget *month_box1 = g_object_new(GTK_TYPE_BOX, "spacing", 6, "homogeneous", true, NULL);
  GtkWidget *month_box2 = g_object_new(GTK_TYPE_BOX, "spacing", 6, "homogeneous", true, NULL);
  const char *const months[12] = {
      _("Jan"), _("Feb"), _("Mar"), _("Apr"), _("May"), _("Jun"),
      _("Jul"), _("Aug"), _("Sep"), _("Oct"), _("Nov"), _("Dec"),
  };
  for_range(i, 0, 12) {
    GtkWidget *btn =
        g_object_new(GTK_TYPE_TOGGLE_BUTTON, "label", months[i], "valign", GTK_ALIGN_CENTER, NULL);
    gtk_widget_add_css_class(btn, "weekday");
    if (i < 6)
      gtk_box_append(GTK_BOX(month_box1), btn);
    else
      gtk_box_append(GTK_BOX(month_box2), btn);
  }
  gtk_box_append(GTK_BOX(month_row_box), month_box1);
  gtk_box_append(GTK_BOX(month_row_box), month_box2);
  self->by_month_row = g_object_new(GTK_TYPE_LIST_BOX_ROW, "child", month_row_box, NULL);
  adw_expander_row_add_row(ADW_EXPANDER_ROW(self->repeat_row), self->by_month_row);

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

ErrandsDateWindow *errands_date_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_DATE_WINDOW, NULL));
}

void errands_date_window_show(ErrandsTask *task) {
  LOG("Date window: Show for '%s'", task->data->uid);
  state.date_window->task = task;

  // Set start date and time

  // Clear start date and time
  errands_date_chooser_reset(state.date_window->start_date_chooser);
  errands_time_chooser_reset(state.date_window->start_time_chooser);

  // Set date
  char *s_date = task->data->start_date;
  // If start date not empty
  if (strcmp(s_date, "")) {
    errands_date_chooser_set_date(state.date_window->start_date_chooser, s_date);
    LOG("Date window: Set date to '%s'", s_date);
  }
  // Set time
  const char *s_time = strstr(s_date, "T");
  if (s_time) {
    s_time++;
    errands_time_chooser_set_time(state.date_window->start_time_chooser, s_time);
    LOG("Date window: Set time to '%s'", s_time);
  }

  // Setup repeat
  bool is_repeated = !strcmp(task->data->rrule, "") ? false : true;
  adw_expander_row_set_enable_expansion(ADW_EXPANDER_ROW(state.date_window->repeat_row),
                                        is_repeated);
  adw_expander_row_set_expanded(ADW_EXPANDER_ROW(state.date_window->repeat_row), is_repeated);
  // Need to select second first if not repeating
  if (!is_repeated)
    adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 2);
  else {
    // Set frequency
    char *freq = get_rrule_value(task->data->rrule, "FREQ");
    if (freq) {
      if (!strcmp(freq, "MINUTELY"))
        adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 0);
      else if (!strcmp(freq, "HOURLY"))
        adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 1);
      else if (!strcmp(freq, "DAILY"))
        adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 2);
      else if (!strcmp(freq, "WEEKLY"))
        adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 3);
      else if (!strcmp(freq, "MONTHLY"))
        adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 4);
      else if (!strcmp(freq, "YEARLY"))
        adw_combo_row_set_selected(ADW_COMBO_ROW(state.date_window->frequency_row), 5);
      LOG("Date window: Set frequency to '%s'", freq);
      free(freq);
    }

    // Set interval
    char *interval_str = get_rrule_value(task->data->rrule, "INTERVAL");
    if (interval_str) {
      if (string_is_number(interval_str)) {
        int interval = atoi(interval_str);
        adw_spin_row_set_value(ADW_SPIN_ROW(state.date_window->interval_row), interval);
      }
      LOG("Date window: Set interval to '%s'", interval_str);
      free(interval_str);
    } else {
      adw_spin_row_set_value(ADW_SPIN_ROW(state.date_window->interval_row), 1);
      LOG("Date window: Set interval to '1'");
    }

    // Set weekdays
    errands_week_chooser_reset(state.date_window->week_chooser);
    errands_week_chooser_set_days(state.date_window->week_chooser, task->data->rrule);

    // Set month
    GtkWidget *month_box_2 = gtk_widget_get_last_child(
        gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(state.date_window->by_month_row)));
    GtkWidget *month_box_1 = gtk_widget_get_prev_sibling(month_box_2);
    GPtrArray *months1 = get_children(month_box_1);
    GPtrArray *months2 = get_children(month_box_2);
    for_range(i, 0, months1->len) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months1->pdata[i]), false);
    }
    for_range(i, 0, months2->len) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months2->pdata[i]), false);
    }
    char *months = get_rrule_value(task->data->rrule, "BYMONTH");
    if (months) {
      int *nums = string_to_int_array(months);
      for (int *i = nums; *i != 0; i++) {
        if (*i <= 6)
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months1->pdata[*i - 1]), true);
        else
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months2->pdata[*i - 7]), true);
      }
      LOG("Date window: Set months to '%s'", months);
      free(months);
      free(nums);
    }
    g_ptr_array_free(months1, true);
    g_ptr_array_free(months2, true);

    // Clear until date
    errands_date_chooser_reset(state.date_window->until_date_chooser);
    // Show count row
    // gtk_widget_set_visible(state.date_window->count_row, true);

    // Set until date
    char *until = get_rrule_value(task->data->rrule, "UNTIL");
    if (until) {
      errands_date_chooser_set_date(state.date_window->until_date_chooser, until);
      LOG("Date window: Set until date to '%s'", until);
      free(until);
    } else {
      // Set count
      char *count_str = get_rrule_value(task->data->rrule, "COUNT");
      if (count_str) {
        if (string_is_number(count_str)) {
          int count = atoi(count_str);
          adw_spin_row_set_value(ADW_SPIN_ROW(state.date_window->count_row), count);
        }
        LOG("Date window: Set count to '%s'", count_str);
        free(count_str);
      } else {
        adw_spin_row_set_value(ADW_SPIN_ROW(state.date_window->count_row), 0);
        LOG("Date window: Set count to infinite");
      }
    }
  }

  // Setup due date and time if not repeated

  // Clear date and time
  errands_date_chooser_reset(state.date_window->due_date_chooser);
  errands_time_chooser_reset(state.date_window->due_time_chooser);

  if (!is_repeated) {
    char *d_date = task->data->due_date;
    // If due date not empty set date
    if (strcmp(d_date, "")) {
      errands_date_chooser_set_date(state.date_window->due_date_chooser, d_date);
      LOG("Date window: Set due date to '%s'", d_date);
    }
    // If time is set
    const char *d_time = strstr(d_date, "T");
    if (d_time) {
      d_time++;
      errands_time_chooser_set_time(state.date_window->due_time_chooser, d_time);
      LOG("Date window: Set due time to '%s'", d_time);
    }
  }

  adw_dialog_present(ADW_DIALOG(state.date_window), GTK_WIDGET(state.main_window));
}

// --- SIGNAL HANDLERS --- //

static void on_errands_date_window_close_cb(ErrandsDateWindow *win, gpointer data) {
  // Set start datetime
  bool start_time_is_set =
      strcmp(gtk_label_get_label(GTK_LABEL(win->start_time_chooser->label)), _("Not Set"));
  bool start_date_is_set =
      strcmp(gtk_label_get_label(GTK_LABEL(win->start_date_chooser->label)), _("Not Set"));
  if (!start_time_is_set && !start_date_is_set) {
    free(win->task->data->start_date);
    win->task->data->start_date = strdup("");
  } else if (start_time_is_set) {
    const char *start_date = errands_date_chooser_get_date(win->start_date_chooser);
    const char *start_time = errands_time_chooser_get_time(win->start_time_chooser);
    str s_dt = str_new(start_date);
    str_append_printf(&s_dt, "T%sZ", start_time);
    free(win->task->data->start_date);
    win->task->data->start_date = strdup(s_dt.str);
    str_free(&s_dt);
  } else if (start_date_is_set && !start_time_is_set) {
    const char *start_date = errands_date_chooser_get_date(win->start_date_chooser);
    free(win->task->data->start_date);
    win->task->data->start_date = strdup(start_date);
  }

  // Set due datetime
  bool repeated = adw_expander_row_get_enable_expansion(ADW_EXPANDER_ROW(win->repeat_row));
  if (!repeated) {
    bool due_time_is_set =
        strcmp(gtk_label_get_label(GTK_LABEL(win->due_time_chooser->label)), _("Not Set"));
    bool due_date_is_set =
        strcmp(gtk_label_get_label(GTK_LABEL(win->due_date_chooser->label)), _("Not Set"));
    if (!due_time_is_set && !due_date_is_set) {
      free(win->task->data->due_date);
      win->task->data->due_date = strdup("");
    } else if (due_time_is_set) {
      const char *due_date = errands_date_chooser_get_date(win->due_date_chooser);
      const char *due_time = errands_time_chooser_get_time(win->due_time_chooser);
      str d_dt = str_new(due_date);
      str_append_printf(&d_dt, "T%sZ", due_time);
      free(win->task->data->due_date);
      win->task->data->due_date = strdup(d_dt.str);
      str_free(&d_dt);
    } else if (due_date_is_set && !due_time_is_set) {
      const char *due_date = errands_date_chooser_get_date(win->due_date_chooser);
      free(win->task->data->due_date);
      win->task->data->due_date = strdup(due_date);
    }
  }

  // Check if repeat is enabled
  if (!repeated) {
    // If not - clean rrule
    free(win->task->data->rrule);
    win->task->data->rrule = strdup("");
  }
  // Generate new rrule
  else {
    // Get frequency
    int frequency = adw_combo_row_get_selected(ADW_COMBO_ROW(win->frequency_row));

    // Create new rrule string
    str rrule = str_new("RRULE:");

    // Set frequency
    if (frequency == 0)
      str_append(&rrule, "FREQ=MINUTELY;");
    else if (frequency == 1)
      str_append(&rrule, "FREQ=HOURLY;");
    else if (frequency == 2)
      str_append(&rrule, "FREQ=DAILY;");
    else if (frequency == 3)
      str_append(&rrule, "FREQ=WEEKLY;");
    else if (frequency == 4)
      str_append(&rrule, "FREQ=MONTHLY;");
    else if (frequency == 5)
      str_append(&rrule, "FREQ=YEARLY;");

    // Set interval
    str_append_printf(&rrule, "INTERVAL=%d;",
                      (int)adw_spin_row_get_value(ADW_SPIN_ROW(win->interval_row)));

    // Set week days
    str_append(&rrule, errands_week_chooser_get_days(win->week_chooser));

    // Set months
    GtkWidget *month_box_2 =
        gtk_widget_get_last_child(gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(win->by_month_row)));
    GtkWidget *month_box_1 = gtk_widget_get_prev_sibling(month_box_2);
    str by_month = str_new("BYMONTH=");
    // First 6 months
    GPtrArray *months_1 = get_children(month_box_1);
    for_range(i, 0, 6) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(months_1->pdata[i]))) {
        if (!strcmp(by_month.str, "BYMONTH="))
          str_append_printf(&by_month, "%d", i + 1);
        else
          str_append_printf(&by_month, ",%d", i + 1);
      }
    }
    g_ptr_array_free(months_1, true);
    // Last 6 months
    GPtrArray *months_2 = get_children(month_box_2);
    for_range(i, 0, 6) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(months_2->pdata[i]))) {
        if (!strcmp(by_month.str, "BYMONTH="))
          str_append_printf(&by_month, "%d", i + 7);
        else
          str_append_printf(&by_month, ",%d", i + 7);
      }
    }
    g_ptr_array_free(months_2, true);
    if (str_eq_c(&by_month, "BYMONTH=")) {
      str_append(&by_month, ";");
      str_append(&rrule, by_month.str);
    }
    str_free(&by_month);

    // Set UNTIL if until date is set
    const char *until_label = gtk_label_get_label(GTK_LABEL(win->until_date_chooser->label));
    if (strcmp(until_label, _("Not Set"))) {
      GDateTime *until_d = gtk_calendar_get_date(GTK_CALENDAR(win->until_date_chooser->calendar));
      char *until_d_str = g_date_time_format(until_d, "%Y%m%d");
      str_append_printf(&rrule, "UNTIL=%s", until_d_str);
      g_free(until_d_str);
      // If start date contains time - add it to the until date
      const char *time = strstr(win->task->data->start_date, "T");
      if (time)
        str_append(&rrule, time);
      str_append(&rrule, ";");
    }

    // Set count if until is not set
    int count = adw_spin_row_get_value(ADW_SPIN_ROW(win->count_row));
    if (count > 0 && !strstr(rrule.str, "UNTIL="))
      str_append_printf(&rrule, "COUNT=%d;", count);

    // Save rrule
    free(win->task->data->rrule);
    win->task->data->rrule = strdup(rrule.str);

    // Cleanup
    str_free(&rrule);
  }

  errands_data_write();
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
  if (selected_freq == 0)
    ending = (char *)C_("Every ...", "minutes");
  else if (selected_freq == 1)
    ending = (char *)C_("Every ...", "hours");
  else if (selected_freq == 2)
    ending = (char *)C_("Every ...", "days");
  else if (selected_freq == 3)
    ending = (char *)C_("Every ...", "weeks");
  else if (selected_freq == 4)
    ending = (char *)C_("Every ...", "months");
  else if (selected_freq == 5)
    ending = (char *)C_("Every ...", "years");

  // Set subtitle
  str subtitle = str_new_printf(_("Repeat every %d %s"), (int)adw_spin_row_get_value(row), ending);
  g_object_set(row, "subtitle", subtitle.str, NULL);
  str_free(&subtitle);
}

static void on_count_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsDateWindow *win) {
  int value = adw_spin_row_get_value(row);
  if (value == 0)
    g_object_set(row, "subtitle", _("Repeat infinitely"), NULL);
  else {
    str subtitle = str_new_printf(_("Repeat %d times"), value);
    g_object_set(row, "subtitle", subtitle.str, NULL);
    str_free(&subtitle);
  }
}
