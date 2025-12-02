#include "data.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"
#include "task-list.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_dialog_close_cb(ErrandsTaskListDateDialog *self);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListDateDialog {
  AdwDialog parent_instance;
  ErrandsTaskListDateDialogDateChooser *start_date_chooser;
  ErrandsTaskListDateDialogTimeChooser *start_time_chooser;
  ErrandsTaskListDateDialogRruleRow *rrule_row;
  ErrandsTaskListDateDialogDateChooser *due_date_chooser;
  ErrandsTaskListDateDialogTimeChooser *due_time_chooser;
  ErrandsTask *current_task;
};

G_DEFINE_TYPE(ErrandsTaskListDateDialog, errands_task_list_date_dialog, ADW_TYPE_DIALOG)

static void errands_task_list_date_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_DATE_DIALOG);
  G_OBJECT_CLASS(errands_task_list_date_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_date_dialog_class_init(ErrandsTaskListDateDialogClass *class) {
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_TIME_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW);
  G_OBJECT_CLASS(class)->dispose = errands_task_list_date_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-date-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, start_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, start_time_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, rrule_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, due_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListDateDialog, due_time_chooser);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
}

static void errands_task_list_date_dialog_init(ErrandsTaskListDateDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListDateDialog *errands_task_list_date_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_date_dialog_show(ErrandsTask *task) {
  LOG("Date Dialog: Show");
  if (!state.main_window->task_list->date_dialog)
    state.main_window->task_list->date_dialog = errands_task_list_date_dialog_new();
  ErrandsTaskListDateDialog *dialog = state.main_window->task_list->date_dialog;
  dialog->current_task = task;
  TaskData *data = task->data;

  // Reset all rows
  errands_task_list_date_dialog_date_chooser_reset(dialog->start_date_chooser);
  errands_task_list_date_dialog_time_chooser_reset(dialog->start_time_chooser);
  errands_task_list_date_dialog_rrule_row_reset(dialog->rrule_row);
  errands_task_list_date_dialog_date_chooser_reset(dialog->due_date_chooser);
  errands_task_list_date_dialog_time_chooser_reset(dialog->due_time_chooser);

  // Set start dt
  LOG("Date Dialog: Set start time");
  icaltimetype start_dt = errands_data_get_time(data->data, DATA_PROP_START_TIME);
  errands_task_list_date_dialog_date_chooser_set_date(dialog->start_date_chooser, start_dt);
  errands_task_list_date_dialog_time_chooser_set_time(dialog->start_time_chooser, start_dt);

  // Set due dt
  LOG("Date Dialog: Set due time");
  icaltimetype due_dt = errands_data_get_time(data->data, DATA_PROP_DUE_TIME);
  errands_task_list_date_dialog_date_chooser_set_date(dialog->due_date_chooser, due_dt);
  errands_task_list_date_dialog_time_chooser_set_time(dialog->due_time_chooser, due_dt);

  // Set rrule
  icalproperty *rrule_prop = icalcomponent_get_first_property(data->data, ICAL_RRULE_PROPERTY);
  if (rrule_prop) {
    LOG("Date Dialog: Set RRULE");
    struct icalrecurrencetype rrule = icalproperty_get_rrule(rrule_prop);
    errands_task_list_date_dialog_rrule_row_set_rrule(dialog->rrule_row, rrule);
  }
  adw_expander_row_set_expanded(ADW_EXPANDER_ROW(dialog->rrule_row), rrule_prop != NULL);

  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(state.main_window));
}
// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskListDateDialog *self) {
  LOG("Date Dialog: Close");

  TaskData *data = self->current_task->data;
  icaltimetype today = icaltime_today();
  bool changed = false;

  // Set start datetime
  icaltimetype curr_sdt = errands_data_get_time(data->data, DATA_PROP_START_TIME);
  icaltimetype new_sdt = ICALTIMETYPE_INITIALIZER;
  icaltimetype new_sd = errands_task_list_date_dialog_date_chooser_get_date(self->start_date_chooser);
  icaltimetype new_st = errands_task_list_date_dialog_time_chooser_get_time(self->start_time_chooser);
  bool new_dd_is_null = icaltime_is_null_date(new_sd);
  bool new_dt_is_null = icaltime_is_null_time(new_st);
  bool curr_ddt_is_null = icaltime_is_null_time(curr_sdt) && icaltime_is_null_date(new_sdt);
  new_sdt.year = new_sd.year;
  new_sdt.month = new_sd.month;
  new_sdt.day = new_sd.day;
  new_sdt.hour = new_st.hour;
  new_sdt.minute = new_st.minute;
  new_sdt.second = new_st.second;
  if (new_dd_is_null) {
    if (new_dt_is_null) {
      if (!curr_ddt_is_null) {
        errands_data_set_time(data->data, DATA_PROP_START_TIME, new_sdt);
        changed = true;
      }
    } else {
      new_sdt.year = today.year;
      new_sdt.month = today.month;
      new_sdt.day = today.day;
      if (icaltime_compare(curr_sdt, new_sdt) != 0) {
        LOG("Date Dialog: Start date is changed to '%s' => '%s'", icaltime_as_ical_string(curr_sdt),
            icaltime_as_ical_string(new_sdt));
        errands_data_set_time(data->data, DATA_PROP_START_TIME, new_sdt);
        changed = true;
      }
    }
  } else {
    if (new_dt_is_null) new_sdt.is_date = true;
    if (icaltime_compare(curr_sdt, new_sdt) != 0) {
      LOG("Date Dialog: Start date is changed '%s' => '%s'", icaltime_as_ical_string(curr_sdt),
          icaltime_as_ical_string(new_sdt));
      errands_data_set_time(data->data, DATA_PROP_START_TIME, new_sdt);
      changed = true;
    }
  }

  // Set due datetime if not repeated
  if (!adw_expander_row_get_expanded(ADW_EXPANDER_ROW(self->rrule_row))) {
    icaltimetype curr_ddt = errands_data_get_time(data->data, DATA_PROP_DUE_TIME);
    icaltimetype new_ddt = ICALTIMETYPE_INITIALIZER;
    icaltimetype new_dd = errands_task_list_date_dialog_date_chooser_get_date(self->due_date_chooser);
    icaltimetype new_dt = errands_task_list_date_dialog_time_chooser_get_time(self->due_time_chooser);
    bool new_dd_is_null = icaltime_is_null_date(new_dd);
    bool new_dt_is_null = icaltime_is_null_time(new_dt);
    bool curr_ddt_is_null = icaltime_is_null_time(curr_ddt) && icaltime_is_null_date(new_ddt);
    new_ddt.year = new_dd.year;
    new_ddt.month = new_dd.month;
    new_ddt.day = new_dd.day;
    new_ddt.hour = new_dt.hour;
    new_ddt.minute = new_dt.minute;
    new_ddt.second = new_dt.second;
    if (new_dd_is_null) {
      if (new_dt_is_null) {
        if (!curr_ddt_is_null) {
          errands_data_set_time(data->data, DATA_PROP_DUE_TIME, new_ddt);
          changed = true;
        }
      } else {
        new_ddt.year = today.year;
        new_ddt.month = today.month;
        new_ddt.day = today.day;
        if (icaltime_compare(curr_ddt, new_ddt) != 0) {
          LOG("Date Dialog: Due date is changed to '%s' => '%s'", icaltime_as_ical_string(curr_ddt),
              icaltime_as_ical_string(new_ddt));
          errands_data_set_time(data->data, DATA_PROP_DUE_TIME, new_ddt);
          changed = true;
        }
      }
    } else {
      if (new_dt_is_null) new_ddt.is_date = true;

      if (icaltime_compare(curr_ddt, new_ddt) != 0) {
        LOG("Date Dialog: Due date is changed '%s' => '%s'", icaltime_as_ical_string(curr_ddt),
            icaltime_as_ical_string(new_ddt));
        errands_data_set_time(data->data, DATA_PROP_DUE_TIME, new_ddt);
        changed = true;
      }
    }
  }

  // Set rrule
  struct icalrecurrencetype old_rrule = ICALRECURRENCETYPE_INITIALIZER;
  struct icalrecurrencetype new_rrule = ICALRECURRENCETYPE_INITIALIZER;
  // Get old and new rrule
  icalproperty *rrule_prop = icalcomponent_get_first_property(data->data, ICAL_RRULE_PROPERTY);
  if (rrule_prop) old_rrule = icalproperty_get_rrule(rrule_prop);
  if (adw_expander_row_get_expanded(ADW_EXPANDER_ROW(self->rrule_row)))
    new_rrule = errands_task_list_date_dialog_rrule_row_get_rrule(self->rrule_row);
  // Compare them and set / remove
  if (!icalrecurrencetype_compare(&new_rrule, &old_rrule)) {
    if (rrule_prop) {
      // Delete rrule if new rrule is not set
      if (new_rrule.freq == ICAL_NO_RECURRENCE) icalcomponent_remove_property(data->data, rrule_prop);
      // Set new rrule
      else icalproperty_set_rrule(rrule_prop, new_rrule);
    } else {
      // Set new rrule
      if (new_rrule.freq != ICAL_NO_RECURRENCE)
        icalcomponent_add_property(data->data, icalproperty_new_rrule(new_rrule));
    }
    changed = true;
  }

  // Write data if changed one of the props
  if (changed) {
    if (self->current_task->data->parent) errands_task_data_sort_sub_tasks(self->current_task->data->parent);
    else errands_data_sort();
    errands_task_update_toolbar(self->current_task);
    errands_sidebar_update_filter_rows(state.main_window->sidebar);
    errands_task_list_reload(state.main_window->task_list, true);
    errands_data_write_list(data->list);
    errands_sync_schedule_task(data);
  }
}
