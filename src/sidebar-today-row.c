#include "data.h"
#include "sidebar.h"

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsSidebarTodayRow {
  GtkListBoxRow parent_instance;
  GtkWidget *counter;
};

G_DEFINE_TYPE(ErrandsSidebarTodayRow, errands_sidebar_today_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_today_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_TODAY_ROW);
  G_OBJECT_CLASS(errands_sidebar_today_row_parent_class)->dispose(gobject);
}

static void errands_sidebar_today_row_class_init(ErrandsSidebarTodayRowClass *class) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-today-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarTodayRow, counter);
  G_OBJECT_CLASS(class)->dispose = errands_sidebar_today_row_dispose;
}

static void errands_sidebar_today_row_init(ErrandsSidebarTodayRow *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsSidebarTodayRow *errands_sidebar_today_row_new() { return g_object_new(ERRANDS_TYPE_SIDEBAR_TODAY_ROW, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_sidebar_today_row_update_counter(ErrandsSidebarTodayRow *row) {
  size_t counter = 0;
  icaltimetype today = icaltime_today();
  for_range(i, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, i);
    g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(list);
    for_range(j, 0, tasks->len) {
      icalcomponent *data = g_ptr_array_index(tasks, j);
      bool deleted = errands_data_get_bool(data, DATA_PROP_DELETED);
      bool completed = !icaltime_is_null_time(errands_data_get_time(data, DATA_PROP_COMPLETED_TIME));
      icaltimetype due_date = errands_data_get_time(data, DATA_PROP_DUE_TIME);
      if (!deleted && !completed && !icaltime_is_null_time(due_date) && icaltime_compare_date_only(due_date, today) < 1)
        counter++;
    }
  }
  gtk_label_set_label(GTK_LABEL(row->counter), counter > 0 ? tmp_str_printf("%zu", counter) : "");
  LOG("Sidebar Today Row: Update counter: %zu", counter);
}
