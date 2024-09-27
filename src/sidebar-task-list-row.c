#include "sidebar-task-list-row.h"
#include "data.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

#include <string.h>

GtkWidget *errands_sidebar_task_list_row_new(TaskListData *data) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

  // Color
  // TODO

  // Label
  GtkWidget *label = gtk_label_new(data->name);
  gtk_box_append(GTK_BOX(box), label);

  // Counter
  GtkWidget *counter = gtk_label_new("");
  gtk_box_append(GTK_BOX(box), counter);
  g_object_set(counter, "hexpand", true, "halign", GTK_ALIGN_END, NULL);
  gtk_widget_add_css_class(counter, "dim-label");
  gtk_widget_add_css_class(counter, "caption");

  // ListBoxRow
  GtkWidget *row = gtk_list_box_row_new();
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
  g_object_set_data(G_OBJECT(row), "list_data", data);
  g_object_set_data(G_OBJECT(row), "label", label);
  g_object_set_data(G_OBJECT(row), "counter", counter);

  errands_sidebar_task_list_row_update_counter(row);

  return row;
}

GtkWidget *errands_sidebar_task_list_row_get(const char *uid) {
  GPtrArray *children = get_children(state.task_lists_list_box);
  for (int i = 0; i < children->len; i++) {
    TaskListData *l_data =
        g_object_get_data(G_OBJECT(children->pdata[i]), "list_data");
    if (!strcmp(l_data->uid, uid))
      return GTK_WIDGET(children->pdata[i]);
  }
  return NULL;
}

void errands_sidebar_task_list_row_update_counter(GtkWidget *row) {
  TaskListData *data = g_object_get_data(G_OBJECT(row), "list_data");
  int c = 0;
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!strcmp(data->uid, td->list_uid))
      c++;
  }
  char *num = g_strdup_printf("%d", c);
  GtkLabel *counter = g_object_get_data(G_OBJECT(row), "counter");
  gtk_label_set_label(counter, c > 0 ? num : "");
  g_free(num);
}

void errands_sidebar_task_list_row_update_title(GtkWidget *row) {
  TaskListData *data = g_object_get_data(G_OBJECT(row), "list_data");
  GtkLabel *label = g_object_get_data(G_OBJECT(row), "label");
  gtk_label_set_label(label, data->name);
}

void on_errands_sidebar_task_list_row_activate(GtkListBox *box,
                                               GtkListBoxRow *row,
                                               gpointer user_data) {
  // Unselect filter rows
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.filters_list_box));

  // Switch to Task List view
  adw_view_stack_set_visible_child_name(ADW_VIEW_STACK(state.stack),
                                        "errands_task_list_page");

  // Filter by uid
  TaskListData *tld = g_object_get_data(G_OBJECT(row), "list_data");
  errands_task_list_filter_by_uid(tld->uid);
  state.current_uid = tld->uid;

  // Show entry
  gtk_widget_set_visible(state.task_list_entry, true);

  LOG("Switch to list '%s'", tld->uid);
}
