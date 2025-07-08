#include "task-list-date-dialog.h"
#include "data/data.h"
#include "state.h"
#include "task-list-date-dialog-date-chooser.h"
#include "task-list-date-dialog-week-chooser.h"
#include "task.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_dialog_close_cb(ErrandsTaskListDateDialog *self);
static void on_freq_changed_cb(AdwComboRow *row, GParamSpec *param, ErrandsTaskListDateDialog *self);
static void on_interval_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsTaskListDateDialog *self);
static void on_count_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsTaskListDateDialog *self);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTaskListDateDialog, errands_task_list_date_dialog, ADW_TYPE_DIALOG)

static void errands_task_list_date_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG);
  G_OBJECT_CLASS(errands_task_list_date_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_class_init(ErrandsTaskListDateDialogClass *class) {
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_TIME_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_WEEK_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_MONTH_CHOOSER);
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, start_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, start_time_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, repeat_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, week_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, month_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, freq_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, interval_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, until_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, count_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, due_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, due_time_chooser);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_freq_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_interval_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_count_changed_cb);
}

static void errands_task_list_date_dialog_init(ErrandsTaskListDateDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialog *errands_task_list_date_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_date_dialog_show(ErrandsTask *task) {
  if (!state.main_window->task_list->date_dialog)
    state.main_window->task_list->date_dialog = errands_task_list_date_dialog_new();
  ErrandsTaskListDateDialog *dialog = state.main_window->task_list->date_dialog;
  dialog->current_task = task;
  TaskData *data = task->data;

  // Reset all rows
  errands_task_list_date_dialog_date_chooser_reset(dialog->start_date_chooser);
  errands_task_list_date_dialog_time_chooser_reset(dialog->start_time_chooser);
  errands_task_list_date_dialog_date_chooser_reset(dialog->due_date_chooser);
  errands_task_list_date_dialog_time_chooser_reset(dialog->due_time_chooser);
  adw_combo_row_set_selected(ADW_COMBO_ROW(dialog->freq_row), 2);
  adw_spin_row_set_value(ADW_SPIN_ROW(dialog->interval_row), 1);
  errands_task_list_date_dialog_week_chooser_reset(dialog->week_chooser);
  errands_task_list_date_dialog_month_chooser_reset(dialog->month_chooser);
  errands_task_list_date_dialog_date_chooser_reset(dialog->until_date_chooser);

  // Set start dt
  icaltimetype start_dt = errands_data_get_time(data, DATA_PROP_START_TIME);
  errands_task_list_date_dialog_date_chooser_set_date(dialog->start_date_chooser, start_dt);
  errands_task_list_date_dialog_time_chooser_set_time(dialog->start_time_chooser, start_dt);

  // Set due dt
  icaltimetype due_dt = errands_data_get_time(data, DATA_PROP_DUE_TIME);
  errands_task_list_date_dialog_date_chooser_set_date(dialog->due_date_chooser, due_dt);
  errands_task_list_date_dialog_time_chooser_set_time(dialog->due_time_chooser, due_dt);

  // Setup repeat
  // const char *rrule = errands_data_get_str(data, DATA_PROP_RRULE);
  // bool is_repeated = rrule ? true : false;
  // adw_expander_row_set_enable_expansion(ADW_EXPANDER_ROW(dialog->repeat_row), is_repeated);
  // adw_expander_row_set_expanded(ADW_EXPANDER_ROW(dialog->repeat_row), is_repeated);
  // if (!is_repeated) {
  //   LOG("Date selfdow: Reccurrence is disabled");
  // } else {
  //   // Set frequency
  //   char *freq = get_rrule_value(rrule, "FREQ");
  //   if (freq) {
  //     if (!strcmp(freq, "MINUTELY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(dialog->frequency_row), 0);
  //     else if (!strcmp(freq, "HOURLY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(dialog->frequency_row), 1);
  //     else if (!strcmp(freq, "DAILY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(dialog->frequency_row), 2);
  //     else if (!strcmp(freq, "WEEKLY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(dialog->frequency_row), 3);
  //     else if (!strcmp(freq, "MONTHLY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(dialog->frequency_row), 4);
  //     else if (!strcmp(freq, "YEARLY"))
  //       adw_combo_row_set_selected(ADW_COMBO_ROW(dialog->frequency_row), 5);
  //     LOG("Date selfdow: Set frequency to '%s'", freq);
  //     free(freq);
  //   }

  //   // Set interval
  //   char *interval_str = get_rrule_value(rrule, "INTERVAL");
  //   if (interval_str) {
  //     if (string_is_number(interval_str)) {
  //       int interval = atoi(interval_str);
  //       adw_spin_row_set_value(ADW_SPIN_ROW(dialog->interval_row), interval);
  //     }
  //     LOG("Date selfdow: Set interval to '%s'", interval_str);
  //     free(interval_str);
  //   } else {
  //     adw_spin_row_set_value(ADW_SPIN_ROW(dialog->interval_row), 1);
  //     LOG("Date selfdow: Set interval to '1'");
  //   }

  //   // Set weekdays
  //   errands_week_chooser_set_days(dialog->week_chooser, rrule);

  //   // Set month
  //   errands_month_chooser_set_months(dialog->month_chooser, rrule);

  //   // Set until date
  //   char *until = get_rrule_value(rrule, "UNTIL");
  //   if (until) {
  //     errands_date_chooser_set_date(dialog->until_date_chooser, until);
  //     LOG("Date selfdow: Set until date to '%s'", until);
  //     free(until);
  //   } else {
  //     // Set count
  //     char *count_str = get_rrule_value(rrule, "COUNT");
  //     if (count_str) {
  //       if (string_is_number(count_str)) {
  //         int count = atoi(count_str);
  //         adw_spin_row_set_value(ADW_SPIN_ROW(dialog->count_row), count);
  //       }
  //       LOG("Date selfdow: Set count to '%s'", count_str);
  //       free(count_str);
  //     } else {
  //       adw_spin_row_set_value(ADW_SPIN_ROW(dialog->count_row), 0);
  //       LOG("Date selfdow: Set count to infinite");
  //     }
  //   }
  // }
  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(state.main_window));
}
// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskListDateDialog *self) {
  TaskData *data = self->current_task->data;

  // Set start datetime
  // bool start_date_is_set =
  //     !strcmp(gtk_label_get_label(GTK_LABEL(self->start_date_chooser->label)), _("Not Set")) ? false : true;
  // bool start_time_is_set =
  //     !strcmp(gtk_label_get_label(GTK_LABEL(self->start_time_chooser->label)), _("Not Set")) ? false : true;
  // LOG("%d %d", start_date_is_set, start_time_is_set);
  // g_autoptr(GString) s_dt = g_string_new("");
  // if (start_date_is_set) {
  //   const char *start_date = errands_date_chooser_get_date(self->start_date_chooser);
  //   g_string_append(s_dt, start_date);
  //   if (start_time_is_set) {
  //     const char *start_time = errands_time_chooser_get_time(self->start_time_chooser);
  //     g_string_append_printf(s_dt, "T%sZ", start_time);
  //   }
  //   errands_data_set_str(data, DATA_PROP_START, s_dt->str);
  // } else {
  //   if (start_time_is_set) {
  //     const char *start_time = errands_time_chooser_get_time(self->start_time_chooser);
  //     char *today = get_today_date();
  //     g_string_append_printf(s_dt, "%sT%sZ", today, start_time);
  //     free(today);
  //   }
  // }
  // errands_data_set_str(data, DATA_PROP_START, s_dt->str);

  // Set due datetime
  // bool due_time_is_set =
  //     !strcmp(gtk_label_get_label(GTK_LABEL(self->due_time_chooser->label)), _("Not Set")) ? false : true;
  // bool due_date_is_set =
  //     !strcmp(gtk_label_get_label(GTK_LABEL(self->due_date_chooser->label)), _("Not Set")) ? false : true;
  // g_autoptr(GString) d_dt = g_string_new("");
  // if (due_date_is_set) {
  //   const char *due_date = errands_date_chooser_get_date(self->due_date_chooser);
  //   g_string_append(d_dt, due_date);
  //   if (due_time_is_set) {
  //     const char *due_time = errands_time_chooser_get_time(self->due_time_chooser);
  //     g_string_append_printf(d_dt, "T%sZ", due_time);
  //   }
  //   errands_data_set_str(data, DATA_PROP_DUE, d_dt->str);
  // } else {
  //   if (due_time_is_set) {
  //     const char *due_time = errands_time_chooser_get_time(self->due_time_chooser);
  //     g_string_append_printf(d_dt, "%sT%sZ", get_today_date(), due_time);
  //   }
  // }
  // errands_data_set_str(data, DATA_PROP_DUE, d_dt->str);

  // Check if repeat is enabled
  // if (!adw_expander_row_get_enable_expansion(ADW_EXPANDER_ROW(self->repeat_row)))
  // If not - clean rrule
  // errands_data_set_str(data, DATA_PROP_RRULE, "");
  // Generate new rrule
  // else {
  //   // Get frequency
  //   guint frequency = adw_combo_row_get_selected(ADW_COMBO_ROW(self->frequency_row));
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
  //   // g_string_append_printf(rrule, "INTERVAL=%d;", (int)adw_spin_row_get_value(ADW_SPIN_ROW(self->interval_row)));

  //   // // Set week days
  //   // g_string_append(rrule, errands_week_chooser_get_days(self->week_chooser));

  //   // // Set months
  //   // g_string_append(rrule, errands_month_chooser_get_months(self->month_chooser));

  //   // // Set UNTIL if until date is set
  //   // const char *until_label = gtk_label_get_label(GTK_LABEL(self->until_date_chooser->label));
  //   // if (strcmp(until_label, _("Not Set"))) {
  //   //   g_autoptr(GDateTime) until_d = gtk_calendar_get_date(GTK_CALENDAR(self->until_date_chooser->calendar));
  //   //   g_autofree gchar *until_d_str = g_date_time_format(until_d, "%Y%m%d");
  //   //   g_string_append_printf(rrule, "UNTIL=%s", until_d_str);
  //   //   // If start date contains time - add it to the until date
  //   //   const char *time = strstr(task_data_get_start(data), "T");
  //   //   if (time)
  //   //     g_string_append(rrule, time);
  //   //   g_string_append(rrule, ";");
  //   // }

  //   // // Set count if until is not set
  //   // int count = adw_spin_row_get_value(ADW_SPIN_ROW(self->count_row));
  //   // if (count > 0 && !strstr(rrule->str, "UNTIL="))
  //   //   g_string_append_printf(rrule, "COUNT=%d;", count);

  //   // Save rrule
  //   if (strcmp(rrule->str, "RRULE:"))
  //     task_data_set_rrule(data, rrule->str);
  // }
  // errands_data_write_list(task_data_get_list(data));
  // Set date button text
  // errands_task_update_toolbar(self->task);
  // TODO: sync
}

static void on_freq_changed_cb(AdwComboRow *row, GParamSpec *param, ErrandsTaskListDateDialog *self) {
  size_t idx = adw_combo_row_get_selected(row);
  gtk_widget_set_visible(GTK_WIDGET(self->week_chooser), idx > 2);
  on_interval_changed_cb(ADW_SPIN_ROW(self->interval_row), NULL, self);
  on_count_changed_cb(ADW_SPIN_ROW(self->count_row), NULL, self);
}

static void on_interval_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsTaskListDateDialog *self) {
  const char *const intervals[] = {C_("Repeat every ...", "minutes"), C_("Repeat every ...", "hours"),
                                   C_("Repeat every ...", "days"),    C_("Repeat every ...", "weeks"),
                                   C_("Repeat every ...", "months"),  C_("Repeat every ...", "years")};
  const uint8_t selected_freq = adw_combo_row_get_selected(ADW_COMBO_ROW(self->freq_row));
  g_autofree gchar *subtitle =
      g_strdup_printf(_("Repeat every %d %s"), (int)adw_spin_row_get_value(row), intervals[selected_freq]);
  g_object_set(row, "subtitle", subtitle, NULL);
}

static void on_count_changed_cb(AdwSpinRow *row, GParamSpec *param, ErrandsTaskListDateDialog *self) {
  const size_t value = adw_spin_row_get_value(row);
  if (value == 0) g_object_set(row, "subtitle", _("Repeat forever"), NULL);
  else {
    g_autofree gchar *subtitle = g_strdup_printf(_("Repeat %zu times"), value);
    g_object_set(row, "subtitle", subtitle, NULL);
  }
}
