#include "sidebar-all-row.h"
#include "data/data.h"
#include "utils.h"

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsSidebarAllRow {
  GtkListBoxRow parent_instance;
  GtkWidget *counter;
};

G_DEFINE_TYPE(ErrandsSidebarAllRow, errands_sidebar_all_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_all_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_ALL_ROW);
  G_OBJECT_CLASS(errands_sidebar_all_row_parent_class)->dispose(gobject);
}

static void errands_sidebar_all_row_class_init(ErrandsSidebarAllRowClass *class) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-all-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarAllRow, counter);
  G_OBJECT_CLASS(class)->dispose = errands_sidebar_all_row_dispose;
}

static void errands_sidebar_all_row_init(ErrandsSidebarAllRow *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsSidebarAllRow *errands_sidebar_all_row_new() { return g_object_new(ERRANDS_TYPE_SIDEBAR_ALL_ROW, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_sidebar_all_row_update_counter(ErrandsSidebarAllRow *row) {
  GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  size_t len = 0;
  for (size_t i = 0; i < tasks->len; i++) {
    TaskData *td = tasks->pdata[i];
    bool deleted = errands_data_get_bool(td, DATA_PROP_DELETED);
    bool trash = errands_data_get_bool(td, DATA_PROP_TRASH);
    bool completed = !icaltime_is_null_time(errands_data_get_time(td, DATA_PROP_COMPLETED_TIME));
    if (!deleted && !trash && !completed) len++;
  }
  char num[64];
  g_snprintf(num, 64, "%zu", len);
  gtk_label_set_label(GTK_LABEL(row->counter), len > 0 ? num : "");
  g_ptr_array_free(tasks, false);
  LOG("All Row: Update counter '%zu'", len);
}
