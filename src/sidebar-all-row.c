#include "sidebar-all-row.h"
#include "data.h"
#include "gtk/gtk.h"
#include "state.h"

G_DEFINE_TYPE(ErrandsSidebarAllRow, errands_sidebar_all_row,
              GTK_TYPE_LIST_BOX_ROW)

static void
errands_sidebar_all_row_class_init(ErrandsSidebarAllRowClass *class) {}

static void errands_sidebar_all_row_init(ErrandsSidebarAllRow *self) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(self), box);
  gtk_box_append(GTK_BOX(box),
                 gtk_image_new_from_icon_name("errands-all-tasks-symbolic"));
  gtk_box_append(GTK_BOX(box), gtk_label_new("All"));

  // Counter
  self->counter = gtk_label_new("");
  gtk_box_append(GTK_BOX(box), self->counter);
  g_object_set(self->counter, "hexpand", true, "halign", GTK_ALIGN_END, NULL);
  gtk_widget_add_css_class(self->counter, "dim-label");
  gtk_widget_add_css_class(self->counter, "caption");
}

ErrandsSidebarAllRow *errands_sidebar_all_row_new() {
  return g_object_new(ERRANDS_TYPE_SIDEBAR_ALL_ROW, NULL);
}

void errands_sidebar_all_row_update_counter() {
  int len = 0;
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!td->deleted && !td->trash)
      len++;
  }
  char *num = g_strdup_printf("%d", len);
  gtk_label_set_label(GTK_LABEL(state.all_row->counter), len > 0 ? num : "");
  g_free(num);
}
