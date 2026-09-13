#include "data.h"
#include "settings.h"
#include "state.h"
#include "task-list.h"

static ErrandsTaskListSortDialog *self = NULL;

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListSortDialog {
  AdwDialog parent_instance;
  bool block_signals;

  // Properties
  gboolean show_completed;
  gboolean show_cancelled;
  ErrandsSettingSortOrder sort_order;
  ErrandsSettingSortType sort_by;
};

G_DEFINE_TYPE(ErrandsTaskListSortDialog, errands_task_list_sort_dialog, ADW_TYPE_DIALOG)

enum {
  PROP_0,

  PROP_SHOW_COMPLETED,
  PROP_SHOW_CANCELLED,
  PROP_SORT_ORDER,
  PROP_SORT_BY,

  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsTaskListSortDialog *self = ERRANDS_TASK_LIST_SORT_DIALOG(object);
  switch (prop_id) {
  case PROP_SHOW_CANCELLED: g_value_set_boolean(value, self->show_cancelled); break;
  case PROP_SHOW_COMPLETED: g_value_set_boolean(value, self->show_completed); break;
  case PROP_SORT_ORDER: g_value_set_int(value, self->sort_order); break;
  case PROP_SORT_BY: g_value_set_int(value, self->sort_by); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsTaskListSortDialog *self = ERRANDS_TASK_LIST_SORT_DIALOG(object);
  switch (prop_id) {
  case PROP_SHOW_CANCELLED: {
    self->show_cancelled = g_value_get_boolean(value);
    if (self->block_signals) return;
    errands_settings_set(SETTING_SHOW_CANCELLED, &self->show_cancelled);
    errands_task_list_filter_tree(state.main_window->task_list,
                                  self->show_cancelled ? GTK_FILTER_CHANGE_LESS_STRICT : GTK_FILTER_CHANGE_MORE_STRICT);
    TODO("call errands_task_item_update() on every task list item to show/hide expander");
  } break;
  case PROP_SHOW_COMPLETED: {
    self->show_completed = g_value_get_boolean(value);
    if (self->block_signals) return;
    errands_settings_set(SETTING_SHOW_COMPLETED, &self->show_completed);
    errands_task_list_filter_tree(state.main_window->task_list, GTK_FILTER_CHANGE_DIFFERENT);
    TODO("call errands_task_item_update() on every task list item to show/hide expander");
  } break;
  case PROP_SORT_ORDER: {
    self->sort_order = g_value_get_int(value);
    if (self->block_signals) return;
    gint sort_order_old = errands_settings_get(SETTING_SORT_ORDER).i;
    if (self->sort_order == sort_order_old) return;
    errands_settings_set(SETTING_SORT_ORDER, &self->sort_order);
    errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_INVERTED);
    LOG("Task List: Sort order changed: %d -> %d", sort_order_old, self->sort_order);
  } break;
  case PROP_SORT_BY: {
    self->sort_by = g_value_get_int(value);
    if (self->block_signals) return;
    gint sort_by_old = errands_settings_get(SETTING_SORT_BY).i;
    if (sort_by_old == self->sort_by) return;
    errands_settings_set(SETTING_SORT_BY, &self->sort_by);
    errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_DIFFERENT);
    LOG("Task List: Sort by changed: %d -> %d", sort_by_old, self->sort_by);
  } break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void errands_task_list_sort_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_SORT_DIALOG);
  G_OBJECT_CLASS(errands_task_list_sort_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_sort_dialog_class_init(ErrandsTaskListSortDialogClass *class) {
  GObjectClass *object_class = G_OBJECT_CLASS(class);
  object_class->dispose = errands_task_list_sort_dialog_dispose;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  obj_properties[PROP_SHOW_COMPLETED] = g_param_spec_boolean(
      "show-completed", "Show Completed", "Whether to show completed tasks", false, G_PARAM_READWRITE);
  obj_properties[PROP_SHOW_CANCELLED] = g_param_spec_boolean(
      "show-cancelled", "Show Cancelled", "Whether to show cancelled tasks", false, G_PARAM_READWRITE);
  obj_properties[PROP_SORT_ORDER] =
      g_param_spec_int("sort-order", "Sort Order", "The sort order", 0, 1, 0, G_PARAM_READWRITE);
  obj_properties[PROP_SORT_BY] = g_param_spec_int("sort-by", "Sort By", "The sort by", 0, 3, 0, G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/task-list-sort-dialog.ui");
}

static void errands_task_list_sort_dialog_init(ErrandsTaskListSortDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListSortDialog *errands_task_list_sort_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_SORT_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_sort_dialog_show() {
  if (!self) self = errands_task_list_sort_dialog_new();
  self->block_signals = true;
  g_object_set(self, "show-completed", errands_settings_get(SETTING_SHOW_COMPLETED).b, NULL);
  g_object_set(self, "show-cancelled", errands_settings_get(SETTING_SHOW_CANCELLED).b, NULL);
  g_object_set(self, "sort-order", errands_settings_get(SETTING_SORT_ORDER).i, NULL);
  g_object_set(self, "sort-by", errands_settings_get(SETTING_SORT_BY).i, NULL);
  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
  self->block_signals = false;
}
