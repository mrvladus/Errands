#include "task-list-date-dialog-month-chooser.h"
#include "glib.h"
#include "utils.h"

struct _ErrandsTaskListDateDialogMonthChooser {
  GtkListBoxRow parent_instance;
  GtkWidget *month_box1;
  GtkWidget *month_box2;
};

G_DEFINE_TYPE(ErrandsTaskListDateDialogMonthChooser, errands_task_list_date_dialog_month_chooser, GTK_TYPE_LIST_BOX_ROW)

static void errands_task_list_date_dialog_month_chooser_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_MONTH_CHOOSER);
  G_OBJECT_CLASS(errands_task_list_date_dialog_month_chooser_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_month_chooser_class_init(ErrandsTaskListDateDialogMonthChooserClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_month_chooser_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog-month-chooser.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogMonthChooser, month_box1);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogMonthChooser, month_box2);
}

static void errands_task_list_date_dialog_month_chooser_init(ErrandsTaskListDateDialogMonthChooser *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialogMonthChooser *errands_task_list_date_dialog_month_chooser_new() {
  return g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_MONTH_CHOOSER, NULL);
}

const char *errands_task_list_date_dialog_month_chooser_get_months(ErrandsTaskListDateDialogMonthChooser *self) {
  static char months[35];
  g_autoptr(GString) by_month = g_string_new("");
  // First 6 months
  GPtrArray *months_1 = get_children(self->month_box1);
  for_range(i, 0, 6) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(months_1->pdata[i]))) {
      if (g_str_equal(by_month->str, "")) g_string_append_printf(by_month, "%d", i + 1);
      else g_string_append_printf(by_month, ",%d", i + 1);
    }
  }
  g_ptr_array_free(months_1, false);
  // Last 6 months
  GPtrArray *months_2 = get_children(self->month_box2);
  for_range(i, 0, 6) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(months_2->pdata[i]))) {
      if (g_str_equal(by_month->str, "")) g_string_append_printf(by_month, "%d", i + 7);
      else g_string_append_printf(by_month, ",%d", i + 7);
    }
  }
  g_ptr_array_free(months_2, false);
  if (!g_str_equal(by_month->str, "")) {
    g_string_prepend(by_month, "BYMONTH=");
    g_string_append(by_month, ";");
  }
  strcpy(months, by_month->str);
  return months;
}

void errands_task_list_date_dialog_month_chooser_set_months(ErrandsTaskListDateDialogMonthChooser *self,
                                                            const char *rrule) {
  GPtrArray *months1 = get_children(self->month_box1);
  GPtrArray *months2 = get_children(self->month_box2);
  char *months = get_rrule_value(rrule, "BYMONTH");
  if (months) {
    int *nums = string_to_int_array(months);
    for (int *i = nums; *i != 0; i++) {
      if (*i <= 6) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months1->pdata[*i - 1]), true);
      else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months2->pdata[*i - 7]), true);
    }
    LOG("Month Chooser: Set months to '%s'", months);
    free(months);
    free(nums);
  }
  g_ptr_array_free(months1, false);
  g_ptr_array_free(months2, false);
}

void errands_task_list_date_dialog_month_chooser_reset(ErrandsTaskListDateDialogMonthChooser *self) {
  GPtrArray *months1 = get_children(self->month_box1);
  GPtrArray *months2 = get_children(self->month_box2);
  for_range(i, 0, months1->len) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months1->pdata[i]), false);
  for_range(i, 0, months2->len) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(months2->pdata[i]), false);
  g_ptr_array_free(months1, false);
  g_ptr_array_free(months2, false);
}
