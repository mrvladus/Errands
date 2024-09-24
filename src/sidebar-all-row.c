#include "sidebar-all-row.h"
#include "data.h"
#include "glib-object.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "state.h"

GtkWidget *errands_sidebar_all_row_new() {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

  // Icon
  GtkWidget *icon = gtk_image_new_from_icon_name("errands-all-tasks-symbolic");
  gtk_box_append(GTK_BOX(box), icon);

  // Label
  GtkWidget *label = gtk_label_new("All");
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

  // Set data
  g_object_set_data(G_OBJECT(row), "counter", counter);

  return row;
}

void errands_sidebar_all_row_update_counter() {
  int len = 0;
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!td->deleted && !td->trash)
      len++;
  }
  char *num = g_strdup_printf("%d", len);
  GtkLabel *counter = g_object_get_data(G_OBJECT(state.all_row), "counter");
  gtk_label_set_label(counter, len > 0 ? num : "");
  g_free(num);
}
