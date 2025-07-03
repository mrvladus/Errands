#include "task-list-date-dialog-week-chooser.h"
#include "utils.h"

struct _ErrandsTaskListDateDialogWeekChooser {
  GtkListBox *parent_instance;
  GtkWidget *week_box;
};

G_DEFINE_TYPE(ErrandsTaskListDateDialogWeekChooser, errands_task_list_date_dialog_week_chooser, GTK_TYPE_LIST_BOX_ROW)

static void errands_task_list_date_dialog_week_chooser_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_WEEK_CHOOSER);
  G_OBJECT_CLASS(errands_task_list_date_dialog_week_chooser_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_week_chooser_class_init(ErrandsTaskListDateDialogWeekChooserClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_week_chooser_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog-week-chooser.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialogWeekChooser, week_box);
}

static void errands_task_list_date_dialog_week_chooser_init(ErrandsTaskListDateDialogWeekChooser *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialogWeekChooser *errands_task_list_date_dialog_week_chooser_new() {
  return g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_WEEK_CHOOSER, NULL);
}

const char *errands_task_list_date_dialog_week_chooser_get_days(ErrandsTaskListDateDialogWeekChooser *self) {
  GPtrArray *week_days = get_children(self->week_box);
  g_autoptr(GString) week_days_str = g_string_new("");
  for (size_t i = 0; i < week_days->len; i++) {
    GtkToggleButton *btn = week_days->pdata[i];
    if (gtk_toggle_button_get_active(btn)) {
      if (i > 0 && strcmp(week_days_str->str, "")) g_string_append(week_days_str, ",");
      if (i == 0) g_string_append(week_days_str, "MO");
      else if (i == 1) g_string_append(week_days_str, "TU");
      else if (i == 2) g_string_append(week_days_str, "WE");
      else if (i == 3) g_string_append(week_days_str, "TH");
      else if (i == 4) g_string_append(week_days_str, "FR");
      else if (i == 5) g_string_append(week_days_str, "SA");
      else if (i == 6) g_string_append(week_days_str, "SU");
    }
  }
  g_ptr_array_free(week_days, false);
  if (strcmp(week_days_str->str, "")) {
    g_string_prepend(week_days_str, "BYDAY=");
    g_string_append(week_days_str, ";");
  }
  static char rrule[27];
  strcpy(rrule, week_days_str->str);
  return rrule;
}

void errands_task_list_date_dialog_week_chooser_set_days(ErrandsTaskListDateDialogWeekChooser *self,
                                                         const char *rrule) {
  GPtrArray *week_days_btns = get_children(self->week_box);
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
  g_ptr_array_free(week_days_btns, false);
}

void errands_task_list_date_dialog_week_chooser_reset(ErrandsTaskListDateDialogWeekChooser *self) {
  GPtrArray *week_days_btns = get_children(self->week_box);
  for_range(i, 0, week_days_btns->len) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(week_days_btns->pdata[i]), false);
  g_ptr_array_free(week_days_btns, false);
}
