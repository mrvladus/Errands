#include "sidebar-task-list-row.h"
#include "data.h"
#include "gtk/gtk.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

#include <stdbool.h>
#include <string.h>

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x,
                           gdouble y, GtkPopover *popover);

G_DEFINE_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row,
              GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_task_list_row_class_init(
    ErrandsSidebarTaskListRowClass *class) {}

static void
errands_sidebar_task_list_row_init(ErrandsSidebarTaskListRow *self) {
  // Box
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(self), box);

  // Color
  // TODO

  // Label
  self->label = gtk_label_new("");
  gtk_box_append(GTK_BOX(box), self->label);

  // Counter
  self->counter = gtk_label_new("");
  gtk_box_append(GTK_BOX(box), self->counter);
  g_object_set(self->counter, "hexpand", true, "halign", GTK_ALIGN_END, NULL);
  gtk_widget_add_css_class(self->counter, "dim-label");
  gtk_widget_add_css_class(self->counter, "caption");

  // Right-click menu
  GMenu *menu = g_menu_new();
  g_menu_append(menu, "Rename", "task-list-row.rename");
  g_menu_append(menu, "Export", "task-list-row.export");
  g_menu_append(menu, "Delete", "task-list-row.delete");

  // Menu popover
  GtkWidget *menu_popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
  g_object_set(menu_popover, "has-arrow", false, "halign", GTK_ALIGN_START,
               NULL);
  gtk_box_append(GTK_BOX(box), menu_popover);
  GtkGesture *ctrl = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(ctrl), 3);
  g_signal_connect(ctrl, "released", G_CALLBACK(on_right_click), menu_popover);
  gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(ctrl));
}

ErrandsSidebarTaskListRow *
errands_sidebar_task_list_row_new(TaskListData *data) {
  ErrandsSidebarTaskListRow *row =
      g_object_new(ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW, NULL);
  row->data = data;
  errands_sidebar_task_list_row_update_title(row);
  errands_sidebar_task_list_row_update_counter(row);
  return row;
}

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(const char *uid) {
  GPtrArray *children = get_children(state.task_lists_list_box);
  for (int i = 0; i < children->len; i++) {
    TaskListData *l_data =
        ((ErrandsSidebarTaskListRow *)children->pdata[i])->data;
    if (!strcmp(l_data->uid, uid))
      return children->pdata[i];
  }
  return NULL;
}

void errands_sidebar_task_list_row_update_counter(
    ErrandsSidebarTaskListRow *row) {
  int c = 0;
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!strcmp(row->data->uid, td->list_uid))
      c++;
  }
  char *num = g_strdup_printf("%d", c);
  gtk_label_set_label(GTK_LABEL(row->counter), c > 0 ? num : "");
  g_free(num);
}

void errands_sidebar_task_list_row_update_title(
    ErrandsSidebarTaskListRow *row) {
  gtk_label_set_label(GTK_LABEL(row->label), row->data->name);
}

void on_errands_sidebar_task_list_row_activate(GtkListBox *box,
                                               ErrandsSidebarTaskListRow *row,
                                               gpointer user_data) {
  // Unselect filter rows
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.filters_list_box));

  // Switch to Task List view
  adw_view_stack_set_visible_child_name(ADW_VIEW_STACK(state.stack),
                                        "errands_task_list_page");

  // Filter by uid
  errands_task_list_filter_by_uid(row->data->uid);
  state.current_uid = row->data->uid;

  // Show entry
  gtk_widget_set_visible(state.task_list_entry, true);

  LOG("Switch to list '%s'", row->data->uid);
}

// --- SIGNALS HANDLERS --- //

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x,
                           gdouble y, GtkPopover *popover) {
  gtk_popover_set_pointing_to(popover, &(GdkRectangle){.x = x, .y = y});
  gtk_popover_popup(popover);
}
