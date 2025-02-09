#include "week-chooser.h"
#include "utils.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE(ErrandsWeekChooser, errands_week_chooser, GTK_TYPE_LIST_BOX_ROW)

static void errands_week_chooser_class_init(ErrandsWeekChooserClass *class) {}

static void errands_week_chooser_init(ErrandsWeekChooser *self) {
  self->week_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_set(self->week_box, "homogeneous", true, NULL);

  const char *const week_days[7] = {C_("Monday", "Mo"),   C_("Tuesday", "Tu"), C_("Wednesday", "We"),
                                    C_("Thursday", "Th"), C_("Friday", "Fr"),  C_("Saturday", "Sa"),
                                    C_("Sunday", "Su")};

  for_range(i, 0, 6) {
    GtkWidget *btn = g_object_new(GTK_TYPE_TOGGLE_BUTTON, "label", week_days[i], "valign", GTK_ALIGN_CENTER, NULL);
    gtk_widget_add_css_class(btn, "weekday");
    gtk_box_append(GTK_BOX(self->week_box), btn);
  }

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  g_object_set(box, "margin-start", 12, "margin-end", 12, "margin-top", 6, "margin-bottom", 6, NULL);
  gtk_box_append(GTK_BOX(box), g_object_new(GTK_TYPE_LABEL, "label", _("Week Days"), "halign", GTK_ALIGN_START, NULL));
  gtk_box_append(GTK_BOX(box), self->week_box);

  g_object_set(self, "child", box, NULL);
}

ErrandsWeekChooser *errands_week_chooser_new() { return g_object_new(ERRANDS_TYPE_WEEK_CHOOSER, NULL); }

const char *errands_week_chooser_get_days(ErrandsWeekChooser *wc) {
  g_autoptr(GPtrArray) week_days = get_children(wc->week_box);
  g_autoptr(GString) week_days_str = g_string_new("");
  for (size_t i = 0; i < week_days->len; i++) {
    GtkToggleButton *btn = week_days->pdata[i];
    if (gtk_toggle_button_get_active(btn)) {
      if (i > 0 && strcmp(week_days_str->str, ""))
        g_string_append(week_days_str, ",");
      if (i == 0)
        g_string_append(week_days_str, "MO");
      else if (i == 1)
        g_string_append(week_days_str, "TU");
      else if (i == 2)
        g_string_append(week_days_str, "WE");
      else if (i == 3)
        g_string_append(week_days_str, "TH");
      else if (i == 4)
        g_string_append(week_days_str, "FR");
      else if (i == 5)
        g_string_append(week_days_str, "SA");
      else if (i == 6)
        g_string_append(week_days_str, "SU");
    }
  }
  if (strcmp(week_days_str->str, "")) {
    g_string_prepend(week_days_str, "BYDAY=");
    g_string_append(week_days_str, ";");
  }
  static char rrule[27];
  strcpy(rrule, week_days_str->str);
  return rrule;
}

void errands_week_chooser_set_days(ErrandsWeekChooser *wc, const char *rrule) {
  g_autoptr(GPtrArray) week_days_btns = get_children(wc->week_box);
  char *week_days = get_rrule_value(rrule, "BYDAY");
  if (week_days) {
    if (string_contains(week_days, "MO"))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[0]), true);
    if (string_contains(week_days, "TU"))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[1]), true);
    if (string_contains(week_days, "WE"))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[2]), true);
    if (string_contains(week_days, "TH"))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[3]), true);
    if (string_contains(week_days, "FR"))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[4]), true);
    if (string_contains(week_days, "SA"))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[5]), true);
    if (string_contains(week_days, "SU"))
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[6]), true);
    LOG("Week Chooser: Set days to '%s'", week_days);
    free(week_days);
  }
}

void errands_week_chooser_reset(ErrandsWeekChooser *wc) {
  g_autoptr(GPtrArray) week_days_btns = get_children(wc->week_box);
  for_range(i, 0, week_days_btns->len) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[i]), false);
  }
}
