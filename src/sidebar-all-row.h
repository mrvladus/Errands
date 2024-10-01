#ifndef ERRANDS_SIDEBAR_ALL_ROW_H
#define ERRANDS_SIDEBAR_ALL_ROW_H

#include <gtk/gtk.h>

#define ERRANDS_TYPE_SIDEBAR_ALL_ROW (errands_sidebar_all_row_get_type())

G_DECLARE_FINAL_TYPE(ErrandsSidebarAllRow, errands_sidebar_all_row, ERRANDS,
                     SIDEBAR_ALL_ROW, GtkListBoxRow)

struct _ErrandsSidebarAllRow {
  GtkListBoxRow parent_instance;
  GtkWidget *counter;
};

ErrandsSidebarAllRow *errands_sidebar_all_row_new();
void errands_sidebar_all_row_update_counter();

#endif // ERRANDS_SIDEBAR_ALL_ROW_H
