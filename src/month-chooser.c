#include "month-chooser.h"
#include "glib.h"
#include "glibconfig.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <string.h>

G_DEFINE_TYPE(ErrandsMonthChooser, errands_month_chooser, GTK_TYPE_LIST_BOX_ROW)

static void errands_month_chooser_class_init(ErrandsMonthChooserClass *class) {}

static void errands_month_chooser_init(ErrandsMonthChooser *self) {
  const char *const months[] = {
      _("Jan"), _("Feb"), _("Mar"), _("Apr"), _("May"), _("Jun"),
      _("Jul"), _("Aug"), _("Sep"), _("Oct"), _("Nov"), _("Dec"),
  };

  self->month_box1 = g_object_new(GTK_TYPE_BOX, "spacing", 6, "homogeneous", true, NULL);
  self->month_box2 = g_object_new(GTK_TYPE_BOX, "spacing", 6, "homogeneous", true, NULL);
  for_range(i, 0, 12) {
    GtkWidget *btn = g_object_new(GTK_TYPE_TOGGLE_BUTTON, "label", months[i], "valign", GTK_ALIGN_CENTER, NULL);
    gtk_widget_add_css_class(btn, "weekday");
    if (i < 6)
      gtk_box_append(GTK_BOX(self->month_box1), btn);
    else
      gtk_box_append(GTK_BOX(self->month_box2), btn);
  }

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  g_object_set(box, "margin-start", 12, "margin-end", 12, "margin-top", 6, "margin-bottom", 6, NULL);
  gtk_box_append(GTK_BOX(box), g_object_new(GTK_TYPE_LABEL, "label", _("Months"), "halign", GTK_ALIGN_START, NULL));
  gtk_box_append(GTK_BOX(box), self->month_box1);
  gtk_box_append(GTK_BOX(box), self->month_box2);

  g_object_set(self, "child", box, NULL);
}

ErrandsMonthChooser *errands_month_chooser_new() { return g_object_new(ERRANDS_TYPE_MONTH_CHOOSER, NULL); }

const char *errands_month_chooser_get_months(ErrandsMonthChooser *mc) {
  static char months[35];
  g_autoptr(GString) by_month = g_string_new("");
  // First 6 months
  g_autoptr(GPtrArray) months_1 = get_children(mc->month_box1);
  for_range(i, 0, 6) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(months_1->pdata[i]))) {
      if (!strcmp(by_month->str, ""))
        g_string_append_printf(by_month, "%d", i + 1);
      else
        g_string_append_printf(by_month, ",%d", i + 1);
    }
  }
  // Last 6 months
  g_autoptr(GPtrArray) months_2 = get_children(mc->month_box2);
  for_range(i, 0, 6) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(months_2->pdata[i]))) {
      if (!strcmp(by_month->str, ""))
        g_string_append_printf(by_month, "%d", i + 7);
      else
        g_string_append_printf(by_month, ",%d", i + 7);
    }
  }
  if (strcmp(by_month->str, "")) {
    g_string_prepend(by_month, "BYMONTH=");
    g_string_append(by_month, ";");
  }
  strcpy(months, by_month->str);
  return months;
}

void errands_month_chooser_set_months(ErrandsMonthChooser *mc, const char *rrule) {
  g_autoptr(GPtrArray) months1 = get_children(mc->month_box1);
  g_autoptr(GPtrArray) months2 = get_children(mc->month_box2);
  char *months = get_rrule_value(rrule, "BYMONTH");
  if (months) {
    int *nums = string_to_int_array(months);
    for (int *i = nums; *i != 0; i++) {
      if (*i <= 6)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months1->pdata[*i - 1]), true);
      else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months2->pdata[*i - 7]), true);
    }
    LOG("Month Chooser: Set months to '%s'", months);
    free(months);
    free(nums);
  }
}

void errands_month_chooser_reset(ErrandsMonthChooser *mc) {
  g_autoptr(GPtrArray) months1 = get_children(mc->month_box1);
  g_autoptr(GPtrArray) months2 = get_children(mc->month_box2);
  for_range(i, 0, months1->len) { gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months1->pdata[i]), false); }
  for_range(i, 0, months2->len) { gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months2->pdata[i]), false); }
}
