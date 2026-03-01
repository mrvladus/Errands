#include "data.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"
#include "task-item.h"
#include "task-list.h"
#include "task.h"
#include "utils.h"
#include "window.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>

static void on_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, ListData *data);
static void on_drop_motion_ctrl_enter_cb(ErrandsSidebarTaskListRow *self);
static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y,
                           ErrandsSidebarTaskListRow *row);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_task_list_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW);
  G_OBJECT_CLASS(errands_sidebar_task_list_row_parent_class)->dispose(gobject);
}

static void errands_sidebar_task_list_row_class_init(ErrandsSidebarTaskListRowClass *klass) {
  G_OBJECT_CLASS(klass)->dispose = errands_sidebar_task_list_row_dispose;

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                              "/io/github/mrvladus/Errands/ui/sidebar-task-list-row.ui");

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebarTaskListRow, color_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebarTaskListRow, counter);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebarTaskListRow, label);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebarTaskListRow, drop_motion_ctrl);

  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_drop_motion_ctrl_enter_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_drop_cb);
}

static void errands_sidebar_task_list_row_init(ErrandsSidebarTaskListRow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_new(ListData *data) {
  g_assert(data);
  LOG_NO_LN("Task List Row '%s': Create ... ", data->uid);
  ErrandsSidebarTaskListRow *row = g_object_new(ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW, NULL);
  row->data = data;
  // Set color
  GdkRGBA color;
  gdk_rgba_parse(&color, errands_data_get_color(data->ical, true));
  gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(row->color_btn), &color);
  g_signal_connect(row->color_btn, "notify::rgba", G_CALLBACK(on_color_changed), data);
  // Update
  errands_sidebar_task_list_row_update(row);
  LOG_NO_PREFIX("Success");
  return row;
}

// ---------- PUBLIC FUNCTIONS ---------- //

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(ListData *data) {
  g_autoptr(GPtrArray) children = get_children(state.main_window->sidebar->task_lists_box);
  for_range(i, 0, children->len) {
    ListData *child_data = ((ErrandsSidebarTaskListRow *)g_ptr_array_index(children, i))->data;
    if (child_data == data) return children->pdata[i];
  }
  return NULL;
}

void errands_sidebar_task_list_row_update(ErrandsSidebarTaskListRow *self) {
  if (!self) return;
  size_t total = 0, completed = 0;
  g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(self->data);
  for_range(i, 0, tasks->len) {
    icalcomponent *ical = g_ptr_array_index(tasks, i);
    CONTINUE_IF(errands_data_get_deleted(ical));
    if (errands_data_is_completed(ical)) completed++;
    total++;
  }
  size_t uncompleted = total - completed;
  gtk_label_set_label(GTK_LABEL(self->counter), uncompleted > 0 ? tmp_str_printf("%zu", uncompleted) : "");
  gtk_label_set_label(GTK_LABEL(self->label), errands_data_get_list_name(self->data->ical));
}

// ---------- CALLBACKS ---------- //

void on_errands_sidebar_task_list_row_activate(GtkListBox *box, ErrandsSidebarTaskListRow *row, gpointer user_data) {
  ErrandsTaskList *task_list = state.main_window->task_list;
  // Unselect filter rows
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.main_window->sidebar->filters_box));
  // Set setting
  LOG("Switch to list '%s'", row->data->uid);
  errands_settings_set(SETTING_LAST_LIST_UID, (void *)row->data->uid);
  errands_task_list_show_task_list(task_list, row->data);
  adw_navigation_split_view_set_show_content(state.main_window->split_view, true);
}

static void on_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, ListData *data) {
  const GdkRGBA *color_rgba = gtk_color_dialog_button_get_rgba(btn);
  char new_color[8];
  gdk_rgba_to_hex_string(color_rgba, new_color);
  errands_data_set_color(data->ical, new_color, true);
  errands_list_data_save(data);
  errands_sync_update_list(data);
}

// --- DND --- //

static guint on_drop_motion_ctrl_enter_timeout_cb(ErrandsSidebarTaskListRow *self) {
  if (!gtk_drop_controller_motion_contains_pointer(self->drop_motion_ctrl)) return G_SOURCE_REMOVE;
  if (errands_sidebar_row_is_selected(self)) return G_SOURCE_REMOVE;
  gtk_widget_activate(GTK_WIDGET(self));

  return G_SOURCE_REMOVE;
}

static void on_drop_motion_ctrl_enter_cb(ErrandsSidebarTaskListRow *self) {
  g_timeout_add_once(500, (GSourceOnceFunc)on_drop_motion_ctrl_enter_timeout_cb, self);
}

static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y,
                           ErrandsSidebarTaskListRow *self) {
  ErrandsTaskItem *drop_item = g_value_get_object(value);
  TaskData *drop_data = errands_task_item_get_data(drop_item);
  ListData *list_data = self->data;
  ListData *old_list_data = drop_data->list;

  bool changing_list = old_list_data != list_data;

  ErrandsTaskItem *drop_item_parent = errands_task_item_get_parent(drop_item);

  // Don't move toplevel task in the same list
  if (!changing_list && !drop_item_parent) return false;

  // Get old parent task
  ErrandsTask *old_parent_task = NULL;
  if (drop_data->parent && drop_item_parent) g_object_get(drop_item_parent, "task-widget", &old_parent_task, NULL);

  // Move data
  bool moved = errands_task_data_move_to_list(drop_data, list_data, NULL);
  if (!moved) return false;

  errands_list_data_save(list_data);
  if (changing_list) errands_list_data_save(old_list_data);

  // Update task model if necessary
  if (drop_data->parent) {
    // Add the item to the current task model
    g_list_store_append(state.main_window->task_list->task_model, drop_item);
    GListModel *parent_model = errands_task_item_get_children_model(drop_item_parent);
    guint idx;
    if (g_list_store_find(G_LIST_STORE(parent_model), drop_item, &idx))
      g_list_store_remove(G_LIST_STORE(parent_model), idx);
    if (drop_item_parent) g_object_notify(G_OBJECT(drop_item_parent), "children-model-is-empty");
    // Set parent to NULL
    drop_data->parent = NULL;
    errands_task_item_set_parent(drop_item, NULL);
  }

  // Update filter
  bool selected = errands_sidebar_row_is_selected(self);
  errands_task_list_filter(state.main_window->task_list,
                           selected ? GTK_FILTER_CHANGE_MORE_STRICT : GTK_FILTER_CHANGE_DIFFERENT);
  if (old_parent_task) errands_task_update_progress(old_parent_task);

  // Update the UI after the operation
  errands_sidebar_task_list_row_update(errands_sidebar_task_list_row_get(old_list_data));
  errands_sidebar_task_list_row_update(self);

  return true;
}
