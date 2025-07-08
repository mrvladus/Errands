#include "task-list-date-dialog-rrule-row.h"
#include "task-list-date-dialog-date-chooser.h"
#include "task-list-date-dialog-month-chooser.h"
#include "task-list-date-dialog-week-chooser.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_freq_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwComboRow *row);
static void on_interval_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwSpinRow *row);
static void on_count_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwSpinRow *row);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListDateDialogRruleRow {
  AdwExpanderRow parent_instance;
  AdwComboRow *freq_row;
  AdwSpinRow *interval_row;
  ErrandsTaskListDateDialogWeekChooser *week_chooser;
  ErrandsTaskListDateDialogMonthChooser *month_chooser;
  ErrandsTaskListDateDialogDateChooser *until_date_chooser;
  AdwSpinRow *count_row;
};

G_DEFINE_TYPE(ErrandsTaskListDateDialogRruleRow, errands_task_list_date_dialog_rrule_row, ADW_TYPE_EXPANDER_ROW)

static void errands_task_list_date_dialog_rrule_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW);
  G_OBJECT_CLASS(errands_task_list_date_dialog_rrule_row_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_rrule_row_class_init(ErrandsTaskListDateDialogRruleRowClass *class) {
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_WEEK_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_MONTH_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER);
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_rrule_row_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog-rrule-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, freq_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, interval_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, week_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, month_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, until_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, count_row);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_freq_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_interval_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_count_changed_cb);
  // gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_date_dialog_rrule_row_reset);
}

static void errands_task_list_date_dialog_rrule_row_init(ErrandsTaskListDateDialogRruleRow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialogRruleRow *errands_task_list_date_dialog_rrule_row_new() {
  return g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW, NULL);
}

// ---------- PUBLIC FUNCTIONS ---------- //

struct icalrecurrencetype errands_task_list_date_dialog_rrule_row_get_rrule(ErrandsTaskListDateDialogRruleRow *self) {
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
  struct icalrecurrencetype rrule = {0};
  return rrule;
}

void errands_task_list_date_dialog_rrule_row_set_rrule(ErrandsTaskListDateDialogRruleRow *self,
                                                       struct icalrecurrencetype rrule) {
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
}

void errands_task_list_date_dialog_rrule_row_reset(ErrandsTaskListDateDialogRruleRow *self) {
  adw_combo_row_set_selected(self->freq_row, 2);
  adw_spin_row_set_value(self->interval_row, 1);
  errands_task_list_date_dialog_week_chooser_reset(self->week_chooser);
  errands_task_list_date_dialog_month_chooser_reset(self->month_chooser);
  errands_task_list_date_dialog_date_chooser_reset(self->until_date_chooser);
}

// ---------- CALLBACKS ---------- //

static void on_freq_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwComboRow *row) {
  size_t idx = adw_combo_row_get_selected(row);
  gtk_widget_set_visible(GTK_WIDGET(self->week_chooser), idx > 2);
  on_interval_changed_cb(self, NULL, self->interval_row);
  on_count_changed_cb(self, NULL, self->count_row);
}

static void on_interval_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwSpinRow *row) {
  const char *const intervals[] = {C_("Repeat every ...", "minutes"), C_("Repeat every ...", "hours"),
                                   C_("Repeat every ...", "days"),    C_("Repeat every ...", "weeks"),
                                   C_("Repeat every ...", "months"),  C_("Repeat every ...", "years")};
  const uint8_t selected_freq = adw_combo_row_get_selected(ADW_COMBO_ROW(self->freq_row));
  g_autofree gchar *subtitle =
      g_strdup_printf(_("Repeat every %d %s"), (int)adw_spin_row_get_value(row), intervals[selected_freq]);
  g_object_set(row, "subtitle", subtitle, NULL);
}

static void on_count_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwSpinRow *row) {
  const size_t value = adw_spin_row_get_value(row);
  if (value == 0) g_object_set(row, "subtitle", _("Repeat forever"), NULL);
  else {
    g_autofree gchar *subtitle = g_strdup_printf(_("Repeat %zu times"), value);
    g_object_set(row, "subtitle", subtitle, NULL);
  }
}
