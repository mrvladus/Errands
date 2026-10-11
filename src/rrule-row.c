#include "config.h"
#include "date-chooser.h"
#include "glib.h"
#include "task-properties-dialog.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <libical-glib/libical-glib.h>
#include <libical/ical.h>

static void reset_week_days(ErrandsRruleRow *self);
static GArray *get_week_days(ErrandsRruleRow *self);
static void set_week_days(ErrandsRruleRow *self, short *days, int n_days);

static void on_freq_changed_cb(ErrandsRruleRow *self, GParamSpec *param, AdwComboRow *row);
static void on_repeat_duration_changed_cb(ErrandsRruleRow *self, GParamSpec *param, AdwComboRow *row);
static void on_interval_changed_cb(ErrandsRruleRow *self, GParamSpec *param, AdwSpinRow *row);
static void on_count_changed_cb(ErrandsRruleRow *self, GParamSpec *param, AdwSpinRow *row);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsRruleRow {
  AdwExpanderRow parent_instance;
  AdwComboRow *freq_row, *repeat_duration;
  AdwSpinRow *interval_row, *count_row;
  GtkBox *week_box;
  GtkListBoxRow *week_chooser;
  ErrandsDateChooser *until_date_chooser;
};

G_DEFINE_TYPE(ErrandsRruleRow, errands_rrule_row, ADW_TYPE_EXPANDER_ROW)

static void errands_rrule_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_RRULE_ROW);
  G_OBJECT_CLASS(errands_rrule_row_parent_class)->dispose(gobject);
}

static void errands_rrule_row_class_init(ErrandsRruleRowClass *class) {
  g_type_ensure(ERRANDS_TYPE_DATE_CHOOSER);
  G_OBJECT_CLASS(class)->dispose = errands_rrule_row_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/rrule-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsRruleRow, freq_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsRruleRow, interval_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsRruleRow, repeat_duration);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsRruleRow, until_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsRruleRow, count_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsRruleRow, week_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsRruleRow, week_box);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_freq_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_repeat_duration_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_interval_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_count_changed_cb);
}

static void errands_rrule_row_init(ErrandsRruleRow *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsRruleRow *errands_rrule_row_new(void) { return g_object_new(ERRANDS_TYPE_RRULE_ROW, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

ICalRecurrence *errands_rrule_row_get_rrule(ErrandsRruleRow *self) {
  g_assert_nonnull(self);

  ICalRecurrence *rrule = i_cal_recurrence_new();
  i_cal_recurrence_set_freq(rrule, adw_combo_row_get_selected(self->freq_row));
  i_cal_recurrence_set_interval(rrule, adw_spin_row_get_value(self->interval_row));
  const guint dur = adw_combo_row_get_selected(self->repeat_duration);
  if (dur == 1) i_cal_recurrence_set_until(rrule, errands_date_chooser_get_dt(self->until_date_chooser));
  else if (dur == 2) i_cal_recurrence_set_count(rrule, adw_spin_row_get_value(self->count_row));
  if (i_cal_recurrence_get_freq(rrule) >= 4) {
    g_autoptr(GArray) days = get_week_days(self);
    i_cal_recurrence_set_by_array(rrule, I_CAL_BY_DAY, days);
  }
  return rrule;
}

void errands_rrule_row_set_rrule(ErrandsRruleRow *self, ICalRecurrence *rrule) {
  if (!rrule) return;
  ICalRecurrenceFrequency freq = i_cal_recurrence_get_freq(rrule);
  if (freq == I_CAL_NO_RECURRENCE) return;
  adw_combo_row_set_selected(self->freq_row, freq < 7 ? freq : 3);
  adw_spin_row_set_value(self->interval_row, i_cal_recurrence_get_interval(rrule));
  g_autoptr(GArray) by_day = i_cal_recurrence_get_by_array(rrule, I_CAL_BY_DAY);
  if (by_day->len > 0) set_week_days(self, (short *)by_day->data, by_day->len);
  else reset_week_days(self);
  ICalTime *until = i_cal_recurrence_get_until(rrule);
  if (!i_cal_time_is_null_date(until)) {
    adw_combo_row_set_selected(self->repeat_duration, 1);
    errands_date_chooser_set_dt(self->until_date_chooser, until);
  } else if (i_cal_recurrence_get_count(rrule) > 0) {
    adw_combo_row_set_selected(self->repeat_duration, 2);
    adw_spin_row_set_value(self->count_row, i_cal_recurrence_get_count(rrule));
  } else {
    adw_combo_row_set_selected(self->repeat_duration, 0);
  }
}

void errands_rrule_row_reset(ErrandsRruleRow *self) {
  adw_combo_row_set_selected(self->freq_row, 3);
  adw_spin_row_set_value(self->interval_row, 1);
  adw_combo_row_set_selected(self->repeat_duration, 0);
  reset_week_days(self);
  errands_date_chooser_reset(self->until_date_chooser);
  adw_spin_row_set_value(self->count_row, 0);
  adw_combo_row_set_selected(self->repeat_duration, 0);
  gtk_widget_set_visible(GTK_WIDGET(self->until_date_chooser), false);
  gtk_widget_set_visible(GTK_WIDGET(self->count_row), false);
}

// ---------- PRIVATE FUNCTIONS ---------- //

static void reset_week_days(ErrandsRruleRow *self) {
  GPtrArray *week_days_btns = get_children(GTK_WIDGET(self->week_box));
  for (guint i = 0; i < week_days_btns->len; i++)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[i]), false);
  g_ptr_array_free(week_days_btns, false);
}

static GArray *get_week_days(ErrandsRruleRow *self) {
  GPtrArray *week_days_btns = get_children(GTK_WIDGET(self->week_box));
  GArray *array = g_array_new(FALSE, FALSE, sizeof(short));
  for (guint i = 0; i < week_days_btns->len; i++) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[i]))) {
      short day;
      if (i == 0) day = I_CAL_MONDAY_WEEKDAY;
      else if (i == 1) day = I_CAL_TUESDAY_WEEKDAY;
      else if (i == 2) day = I_CAL_WEDNESDAY_WEEKDAY;
      else if (i == 3) day = I_CAL_THURSDAY_WEEKDAY;
      else if (i == 4) day = I_CAL_FRIDAY_WEEKDAY;
      else if (i == 5) day = I_CAL_SATURDAY_WEEKDAY;
      else day = I_CAL_SUNDAY_WEEKDAY;
      g_array_append_val(array, day);
    }
  }
  g_ptr_array_free(week_days_btns, false);
  return array;
}

static void set_week_days(ErrandsRruleRow *self, short *days, int n_days) {
  reset_week_days(self);
  GPtrArray *week_days_btns = get_children(GTK_WIDGET(self->week_box));
  for (int i = 0; i < n_days; i++) {
    icalrecurrencetype_weekday day = icalrecurrencetype_day_day_of_week(days[i]);
    switch (day) {
    case ICAL_NO_WEEKDAY: break;
    case ICAL_SUNDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[6]), true); break;
    case ICAL_MONDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[0]), true); break;
    case ICAL_TUESDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[1]), true); break;
    case ICAL_WEDNESDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[2]), true); break;
    case ICAL_THURSDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[3]), true); break;
    case ICAL_FRIDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[4]), true); break;
    case ICAL_SATURDAY_WEEKDAY: gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[5]), true); break;
    }
  }
  g_ptr_array_free(week_days_btns, false);
}

// ---------- CALLBACKS ---------- //

static void on_freq_changed_cb(ErrandsRruleRow *self, GParamSpec *param, AdwComboRow *row) {
  size_t idx = adw_combo_row_get_selected(row);
  gtk_widget_set_visible(GTK_WIDGET(self->week_chooser), idx >= 4);
  on_interval_changed_cb(self, NULL, self->interval_row);
  on_count_changed_cb(self, NULL, self->count_row);
}

static void on_interval_changed_cb(ErrandsRruleRow *self, GParamSpec *param, AdwSpinRow *row) {
  const char *const intervals[] = {C_("Repeat every ...", "seconds"), C_("Repeat every ...", "minutes"),
                                   C_("Repeat every ...", "hours"),   C_("Repeat every ...", "days"),
                                   C_("Repeat every ...", "weeks"),   C_("Repeat every ...", "months"),
                                   C_("Repeat every ...", "years")};
  const guint selected_freq = adw_combo_row_get_selected(self->freq_row);
  const char *subtitle = tmp_str_printf(_("Repeat every %d %s"), (int)adw_spin_row_get_value(row), intervals[selected_freq]);
  g_object_set(row, "subtitle", subtitle, NULL);
}

static void on_count_changed_cb(ErrandsRruleRow *self, GParamSpec *param, AdwSpinRow *row) {
  const size_t value = adw_spin_row_get_value(row);
  const char *subtitle = value == 0 ? _("Repeat forever") : tmp_str_printf(_("Repeat %zu times"), value);
  g_object_set(row, "subtitle", subtitle, NULL);
}

static void on_repeat_duration_changed_cb(ErrandsRruleRow *self, GParamSpec *param, AdwComboRow *row) {
  const guint selected_dur = adw_combo_row_get_selected(row);
  gtk_widget_set_visible(GTK_WIDGET(self->until_date_chooser), selected_dur == 1);
  gtk_widget_set_visible(GTK_WIDGET(self->count_row), selected_dur == 2);
}
