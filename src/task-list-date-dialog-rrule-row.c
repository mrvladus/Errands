#include "data.h"
#include "task-list.h"
#include "utils.h"
#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void reset_week_days(ErrandsTaskListDateDialogRruleRow *self);
static void get_week_days(ErrandsTaskListDateDialogRruleRow *self, short days[]);
static void set_week_days(ErrandsTaskListDateDialogRruleRow *self, short days[]);

static void on_freq_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwComboRow *row);
static void on_repeat_duration_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwComboRow *row);
static void on_interval_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwSpinRow *row);
static void on_count_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwSpinRow *row);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListDateDialogRruleRow {
  AdwExpanderRow parent_instance;
  AdwComboRow *freq_row, *repeat_duration;
  AdwSpinRow *interval_row, *count_row;
  GtkBox *week_box;
  GtkListBoxRow *week_chooser;
  ErrandsTaskListDateDialogDateChooser *until_date_chooser;
};

G_DEFINE_TYPE(ErrandsTaskListDateDialogRruleRow, errands_task_list_date_dialog_rrule_row, ADW_TYPE_EXPANDER_ROW)

static void errands_task_list_date_dialog_rrule_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW);
  G_OBJECT_CLASS(errands_task_list_date_dialog_rrule_row_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_rrule_row_class_init(ErrandsTaskListDateDialogRruleRowClass *class) {
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER);
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_rrule_row_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog-rrule-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, freq_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, interval_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, repeat_duration);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, until_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, count_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, week_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogRruleRow, week_box);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_freq_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_repeat_duration_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_interval_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_count_changed_cb);
}

static void errands_task_list_date_dialog_rrule_row_init(ErrandsTaskListDateDialogRruleRow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialogRruleRow *errands_task_list_date_dialog_rrule_row_new() {
  return g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW, NULL);
}

// ---------- PUBLIC FUNCTIONS ---------- //

struct icalrecurrencetype errands_task_list_date_dialog_rrule_row_get_rrule(ErrandsTaskListDateDialogRruleRow *self) {
  struct icalrecurrencetype rrule = ICALRECURRENCETYPE_INITIALIZER;
  icalrecurrencetype_clear(&rrule);
  rrule.freq = adw_combo_row_get_selected(self->freq_row);
  rrule.interval = adw_spin_row_get_value(self->interval_row);
  if (rrule.freq >= 4) get_week_days(self, rrule.by_day);
  if (adw_combo_row_get_selected(self->repeat_duration) == 0)
    rrule.until = errands_task_list_date_dialog_date_chooser_get_date(self->until_date_chooser);
  else rrule.count = adw_spin_row_get_value(self->count_row);

  return rrule;
}

void errands_task_list_date_dialog_rrule_row_set_rrule(ErrandsTaskListDateDialogRruleRow *self,
                                                       struct icalrecurrencetype rrule) {
  if (rrule.freq == ICAL_NO_RECURRENCE) return;
  LOG("Set RRULE: %s", icalrecurrencetype_as_string(&rrule));
  adw_combo_row_set_selected(self->freq_row, rrule.freq < 7 ? rrule.freq : 3);
  adw_spin_row_set_value(self->interval_row, rrule.interval);
  set_week_days(self, rrule.by_day);
  if (icaltime_is_null_date(rrule.until)) adw_spin_row_set_value(self->count_row, rrule.count);
  else errands_task_list_date_dialog_date_chooser_set_date(self->until_date_chooser, rrule.until);
}

void errands_task_list_date_dialog_rrule_row_reset(ErrandsTaskListDateDialogRruleRow *self) {
  adw_combo_row_set_selected(self->freq_row, 3);
  adw_spin_row_set_value(self->interval_row, 1);
  adw_combo_row_set_selected(self->repeat_duration, 0);
  reset_week_days(self);
  errands_task_list_date_dialog_date_chooser_reset(self->until_date_chooser);
}

// ---------- PRIVATE FUNCTIONS ---------- //

static void reset_week_days(ErrandsTaskListDateDialogRruleRow *self) {
  GPtrArray *week_days_btns = get_children(GTK_WIDGET(self->week_box));
  for_range(i, 0, week_days_btns->len) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[i]), false);
  g_ptr_array_free(week_days_btns, false);
}

static void get_week_days(ErrandsTaskListDateDialogRruleRow *self, short days[]) {
  GPtrArray *week_days_btns = get_children(GTK_WIDGET(self->week_box));
  uint8_t curr = 0;
  for (size_t i = 0; i < week_days_btns->len; ++i) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[i]))) {
      if (i == 0) days[curr] = ICAL_MONDAY_WEEKDAY;
      else if (i == 1) days[curr] = ICAL_TUESDAY_WEEKDAY;
      else if (i == 2) days[curr] = ICAL_WEDNESDAY_WEEKDAY;
      else if (i == 3) days[curr] = ICAL_THURSDAY_WEEKDAY;
      else if (i == 4) days[curr] = ICAL_FRIDAY_WEEKDAY;
      else if (i == 5) days[curr] = ICAL_SATURDAY_WEEKDAY;
      else if (i == 6) days[curr] = ICAL_SUNDAY_WEEKDAY;
      curr++;
    }
  }
  days[curr] = ICAL_RECURRENCE_ARRAY_MAX;
  g_ptr_array_free(week_days_btns, false);
}

static void set_week_days(ErrandsTaskListDateDialogRruleRow *self, short days[]) {
  if (days[0] != ICAL_RECURRENCE_ARRAY_MAX) {
    reset_week_days(self);
    GPtrArray *week_days_btns = get_children(GTK_WIDGET(self->week_box));
    for (size_t i = 0; i < ICAL_BY_DAY_SIZE && days[i] != ICAL_RECURRENCE_ARRAY_MAX; i++) {
      icalrecurrencetype_weekday day = icalrecurrencetype_day_day_of_week(days[i]);
      switch (day) {
      case ICAL_NO_WEEKDAY: break;
      case ICAL_SUNDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[6]), true); break;
      case ICAL_MONDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[0]), true); break;
      case ICAL_TUESDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[1]), true); break;
      case ICAL_WEDNESDAY_WEEKDAY:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[2]), true);
        break;
      case ICAL_THURSDAY_WEEKDAY:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[3]), true);
        break;
      case ICAL_FRIDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[4]), true); break;
      case ICAL_SATURDAY_WEEKDAY:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[5]), true);
        break;
      }
    }
    g_ptr_array_free(week_days_btns, false);
  };
}

// ---------- CALLBACKS ---------- //

static void on_freq_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwComboRow *row) {
  size_t idx = adw_combo_row_get_selected(row);
  gtk_widget_set_visible(GTK_WIDGET(self->week_chooser), idx >= 4);
  on_interval_changed_cb(self, NULL, self->interval_row);
  on_count_changed_cb(self, NULL, self->count_row);
}

static void on_interval_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwSpinRow *row) {
  const char *const intervals[] = {C_("Repeat every ...", "seconds"), C_("Repeat every ...", "minutes"),
                                   C_("Repeat every ...", "hours"),   C_("Repeat every ...", "days"),
                                   C_("Repeat every ...", "weeks"),   C_("Repeat every ...", "months"),
                                   C_("Repeat every ...", "years")};
  const uint8_t selected_freq = adw_combo_row_get_selected(self->freq_row);
  const char *subtitle =
      tmp_str_printf(_("Repeat every %d %s"), (int)adw_spin_row_get_value(row), intervals[selected_freq]);
  g_object_set(row, "subtitle", subtitle, NULL);
}

static void on_count_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param, AdwSpinRow *row) {
  const size_t value = adw_spin_row_get_value(row);
  if (value == 0) g_object_set(row, "subtitle", _("Repeat forever"), NULL);
  else {
    const char *subtitle = tmp_str_printf(_("Repeat %zu times"), value);
    g_object_set(row, "subtitle", subtitle, NULL);
  }
}

static void on_repeat_duration_changed_cb(ErrandsTaskListDateDialogRruleRow *self, GParamSpec *param,
                                          AdwComboRow *row) {
  const uint8_t selected_dur = adw_combo_row_get_selected(self->repeat_duration);
  gtk_widget_set_visible(GTK_WIDGET(self->until_date_chooser), selected_dur == 0);
}
