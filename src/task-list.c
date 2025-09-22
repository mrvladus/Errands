#include "task-list.h"
#include "data/data.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtknoselection.h"
#include "gtk/gtkshortcut.h"
#include "settings.h"
#include "state.h"
#include "task.h"
#include "utils.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libical/ical.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

typedef enum {
  EXPAND_DUE,
  EXPAND_SEARCH,
} ExpandType;

static GPtrArray *reload_all_tasks(ErrandsTaskList *self);
static void expand_rows(ErrandsTaskList *self, ExpandType type);
static void restore_expanded_rows(ErrandsTaskList *self);
static bool task_is_due(TaskData *data);
static bool task_or_descendants_is_due(TaskData *data, GPtrArray *all_tasks);
static gboolean task_or_descendants_match_search_query(TaskData *data, const char *query, GPtrArray *all_tasks);

static void setup_flat_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static void bind_flat_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static void setup_tree_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static void bind_tree_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static GListModel *create_child_model_func(gpointer item, gpointer user_data);
static void on_list_view_activate_cb(GtkListView *self, guint position, gpointer user_data);
static void on_task_list_entry_activated_cb(AdwEntryRow *entry, ErrandsTaskList *self);
static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry);

static gboolean base_toplevel_filter_func(GObject *item);
static gboolean base_today_filter_func(GObject *item);
static gboolean today_filter_func(GObject *item, ErrandsTaskList *self);
static gboolean search_filter_func(GObject *item, ErrandsTaskList *self);

static gint master_sort_func(gconstpointer a, gconstpointer b, gpointer user_data);
static gboolean master_filter_func(GObject *item, ErrandsTaskList *self);
static gboolean all_tasks_filter_func(GObject *item, ErrandsTaskList *self);

static void on_test_cb() {
  // GListStore *model = state.main_window->task_list->tasks_model;
  // for_range(i, 0, 1000) {
  //   char num[64];
  //   sprintf(num, "%zu", i);
  //   TaskData *td = task_data_new(state.main_window->task_list->data, num, NULL);
  //   // g_hash_table_insert(tdata, g_strdup(errands_data_get_str(td, DATA_PROP_UID)), td);
  //   g_list_store_append(model, task_data_as_gobject(td));
  // }
}

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)

static void errands_task_list_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST);
  G_OBJECT_CLASS(errands_task_list_parent_class)->dispose(gobject);
}

static void errands_task_list_class_init(ErrandsTaskListClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/io/github/mrvladus/Errands/ui/task-list.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, title);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, search_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, search_bar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, search_entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry_rev);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, task_list);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_sort_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_search_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_list_view_activate_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_test_cb);
}

static void errands_task_list_init(ErrandsTaskList *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(self->search_bar), GTK_EDITABLE(self->search_entry));

  reload_all_tasks(self);
  self->base_model = g_list_store_new(G_TYPE_OBJECT);

  // Base today tasks filter
  self->base_today_filter = gtk_custom_filter_new((GtkCustomFilterFunc)base_today_filter_func, NULL, NULL);
  self->base_today_filter_model =
      gtk_filter_list_model_new(G_LIST_MODEL(self->base_model), GTK_FILTER(self->base_today_filter));

  // Base toplevel filter
  self->base_toplevel_filter = gtk_custom_filter_new((GtkCustomFilterFunc)base_toplevel_filter_func, NULL, NULL);
  self->base_toplevel_filter_model =
      gtk_filter_list_model_new(G_LIST_MODEL(self->base_model), GTK_FILTER(self->base_toplevel_filter));

  // Flat factory
  self->flat_tasks_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(self->flat_tasks_factory, "setup", G_CALLBACK(setup_flat_listitem_cb), NULL);
  g_signal_connect(self->flat_tasks_factory, "bind", G_CALLBACK(bind_flat_listitem_cb), NULL);

  // Tree factory
  self->tree_tasks_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(self->tree_tasks_factory, "setup", G_CALLBACK(setup_tree_listitem_cb), NULL);
  g_signal_connect(self->tree_tasks_factory, "bind", G_CALLBACK(bind_tree_listitem_cb), NULL);

  self->tree_model = gtk_tree_list_model_new(G_LIST_MODEL(self->base_toplevel_filter_model), false, false,
                                             create_child_model_func, NULL, NULL);

  // Sort model
  self->master_sorter = gtk_tree_list_row_sorter_new(GTK_SORTER(gtk_custom_sorter_new(master_sort_func, NULL, NULL)));
  self->master_sort_model = gtk_sort_list_model_new(G_LIST_MODEL(self->tree_model), GTK_SORTER(self->master_sorter));

  // Master filter
  self->master_filter = gtk_custom_filter_new((GtkCustomFilterFunc)master_filter_func, self, NULL);
  self->master_filter_model =
      gtk_filter_list_model_new(G_LIST_MODEL(self->master_sort_model), GTK_FILTER(self->master_filter));

  // Today filter
  // self->today_filter = gtk_custom_filter_new((GtkCustomFilterFunc)today_filter_func, self, NULL);
  // self->today_filter_model =
  //     gtk_filter_list_model_new(G_LIST_MODEL(self->toplevel_filter_model), GTK_FILTER(self->today_filter));
  // Search filter
  // self->search_filter = gtk_custom_filter_new((GtkCustomFilterFunc)search_filter_func, self, NULL);
  // self->search_filter_model =
  //     gtk_filter_list_model_new(G_LIST_MODEL(self->today_filter_model), GTK_FILTER(self->search_filter));

  self->selection_model = gtk_no_selection_new(G_LIST_MODEL(self->master_filter_model));

  // List View
  gtk_list_view_set_model(GTK_LIST_VIEW(self->task_list), GTK_SELECTION_MODEL(self->selection_model));
  gtk_list_view_set_factory(GTK_LIST_VIEW(self->task_list), self->tree_tasks_factory);

  tb_log("Task List: Created");
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

static void setup_flat_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  ErrandsTask *task = errands_task_new();
  gtk_list_item_set_child(list_item, GTK_WIDGET(task));
}

static void bind_flat_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  GObject *item = gtk_list_item_get_item(list_item);
  ErrandsTask *task = ERRANDS_TASK(gtk_list_item_get_child(list_item));
  g_assert(ERRANDS_IS_TASK(task));
  TaskData *task_data = g_object_get_data(item, "data");
  errands_task_set_data(task, task_data);
}

static void setup_tree_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  GtkWidget *expander = gtk_tree_expander_new();
  g_object_set(expander, "margin-top", 3, "margin-bottom", 3, "hide-expander", true, "indent-for-icon", false, NULL);
  ErrandsTask *task = errands_task_new();
  gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), GTK_WIDGET(task));
  gtk_list_item_set_focusable(list_item, true);
  gtk_list_item_set_child(list_item, GTK_WIDGET(expander));
}

static void expand_row_idle_cb(gpointer data) {
  GtkTreeListRow *row = GTK_TREE_LIST_ROW(data);
  gtk_tree_list_row_set_expanded(row, true);
  g_object_unref(row);
}

static void bind_tree_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  GtkTreeListRow *row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(list_item));
  g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(row);
  if (!model_item) return;
  // Get the expander and its child task widget
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(list_item));
  // Set the row on the expander
  gtk_tree_expander_set_list_row(expander, row); // create_child_model_func is called here
  g_object_set_data(G_OBJECT(row), "list-item", list_item);
  ErrandsTask *task = ERRANDS_TASK(gtk_tree_expander_get_child(expander));
  g_object_set_data(G_OBJECT(task), "model-item", model_item);
  g_object_set_data(G_OBJECT(task), "row", row);
  // Set task widget so we can access it in on_list_view_activate()
  g_object_set_data(G_OBJECT(model_item), "task", task);
  // Set the task data
  TaskData *task_data = g_object_get_data(model_item, "data");
  errands_task_set_data(task, task_data);
  // Expand row
  bool expanded = errands_data_get_bool(task_data, DATA_PROP_EXPANDED);
  bool is_expandable = gtk_tree_list_row_is_expandable(row);
  // Add at idle because otherwise will srew-up all list
  if (is_expandable && expanded) g_idle_add_once((GSourceOnceFunc)expand_row_idle_cb, g_object_ref(row));
}

// Expand task on click on row
static void on_list_view_activate_cb(GtkListView *self, guint position, gpointer user_data) {
  // Get row
  GListModel *selection_model = G_LIST_MODEL(gtk_list_view_get_model(self));
  g_autoptr(GtkTreeListRow) row = GTK_TREE_LIST_ROW(g_list_model_get_item(selection_model, position));
  if (!row) return;
  if (!gtk_tree_list_row_is_expandable(row)) return;
  // Set new expanded state
  bool expanded = !gtk_tree_list_row_get_expanded(row);
  gtk_tree_list_row_set_expanded(row, expanded);
  g_autoptr(GObject) item = gtk_tree_list_row_get_item(row);
  TaskData *task_data = g_object_get_data(item, "data");
  errands_data_set_bool(task_data, DATA_PROP_EXPANDED, expanded);
  ErrandsTask *task = g_object_get_data(item, "task");
  errands_task_set_data(task, task_data);
  errands_data_write_list(task_data_get_list(task_data));
}

static GListModel *create_child_model_func(gpointer item, gpointer user_data) {
  return g_object_ref(g_object_get_data(G_OBJECT(item), "children"));
}

// --- SORT AND FILTER CALLBACKS --- //

static gboolean base_toplevel_filter_func(GObject *item) {
  return errands_data_get_str(g_object_get_data(item, "data"), DATA_PROP_PARENT) ? false : true;
}

static gboolean base_today_filter_func(GObject *item) {
  TaskData *data = g_object_get_data(item, "data");
  bool has_parent = errands_data_get_str(data, DATA_PROP_PARENT) != NULL;
  return task_is_due(data) && !has_parent;
}

static gboolean today_filter_func(GObject *item, ErrandsTaskList *self) {
  g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
  if (!model_item) return false;
  TaskData *data = g_object_get_data(model_item, "data");
  if (self->page == ERRANDS_TASK_LIST_PAGE_TODAY)
    if (!task_or_descendants_is_due(data, self->all_tasks)) return false;

  return true;
}

static gboolean search_filter_func(GObject *item, ErrandsTaskList *self) {
  g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
  if (!model_item) return false;
  TaskData *data = g_object_get_data(model_item, "data");
  if (!data) return false;
  if (self->search_query && !g_str_equal(self->search_query, ""))
    if (!task_or_descendants_match_search_query(data, self->search_query, self->all_tasks)) return false;

  return true;
}

static gboolean master_filter_func(GObject *item, ErrandsTaskList *self) {
  g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
  if (!model_item) return false;
  TaskData *data = g_object_get_data(model_item, "data");
  // Completed
  if (!errands_settings_get("show_completed", SETTING_TYPE_BOOL).b)
    if (!icaltime_is_null_date(errands_data_get_time(data, DATA_PROP_COMPLETED_TIME))) return false;
  // Toplevel
  if (self->page == ERRANDS_TASK_LIST_PAGE_TASK_LIST) {
    ListData *list_data = task_data_get_list(data);
    if (list_data != self->data) return false;
  }

  return true;
}

static gboolean all_tasks_filter_func(GObject *item, ErrandsTaskList *self) {
  g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
  if (!model_item) return false;
  TaskData *data = g_object_get_data(model_item, "data");
  // Completed
  if (!errands_settings_get("show_completed", SETTING_TYPE_BOOL).b)
    if (!icaltime_is_null_date(errands_data_get_time(data, DATA_PROP_COMPLETED_TIME))) return false;

  return true;
}

static gint master_sort_func(gconstpointer a, gconstpointer b, gpointer user_data) {
  TaskData *data_a = g_object_get_data(G_OBJECT(a), "data");
  TaskData *data_b = g_object_get_data(G_OBJECT(b), "data");

  if (!data_a || !data_b) return 0;

  // Completion sort first
  gboolean completed_a = !icaltime_is_null_date(errands_data_get_time(data_a, DATA_PROP_COMPLETED_TIME));
  gboolean completed_b = !icaltime_is_null_date(errands_data_get_time(data_b, DATA_PROP_COMPLETED_TIME));
  if (completed_a != completed_b) return completed_a - completed_b;

  // Then apply global sort
  switch (errands_settings_get("sort_by", SETTING_TYPE_INT).i) {
  case SORT_TYPE_CREATION_DATE: {
    icaltimetype creation_date_a = errands_data_get_time(data_a, DATA_PROP_CREATED_TIME);
    icaltimetype creation_date_b = errands_data_get_time(data_b, DATA_PROP_CREATED_TIME);
    return icaltime_compare(creation_date_b, creation_date_a);
  }
  case SORT_TYPE_DUE_DATE: {
    icaltimetype due_a = errands_data_get_time(data_a, DATA_PROP_DUE_TIME);
    icaltimetype due_b = errands_data_get_time(data_b, DATA_PROP_DUE_TIME);
    bool null_a = icaltime_is_null_time(due_a);
    bool null_b = icaltime_is_null_time(due_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(due_a, due_b);
  }
  case SORT_TYPE_PRIORITY: {
    int p_a = errands_data_get_int(data_a, DATA_PROP_PRIORITY);
    int p_b = errands_data_get_int(data_b, DATA_PROP_PRIORITY);
    return p_b - p_a;
  }
  case SORT_TYPE_START_DATE: {
    icaltimetype start_a = errands_data_get_time(data_a, DATA_PROP_START_TIME);
    icaltimetype start_b = errands_data_get_time(data_b, DATA_PROP_START_TIME);
    bool null_a = icaltime_is_null_time(start_a);
    bool null_b = icaltime_is_null_time(start_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(start_a, start_b);
  }
  default: return 0;
  }
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_load_tasks(ErrandsTaskList *self) {
  tb_log("Task List: Loading tasks into model");
  // Create hash table for quick task lookup
  self->tasks_items = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
  // Single pass to populate hash table and identify root tasks
  GPtrArray *root_tasks = g_ptr_array_new_with_free_func(g_object_unref);
  GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  const size_t len = tasks->len;
  for (size_t i = 0; i < len; i++) {
    TaskData *td = tasks->pdata[i];
    const char *task_uid = errands_data_get_str(td, DATA_PROP_UID);
    GObject *item = task_data_as_gobject(td);
    g_hash_table_insert(self->tasks_items, g_strdup(task_uid), item);
    // Identify root tasks (no parent)
    const char *parent_uid = errands_data_get_str(td, DATA_PROP_PARENT);
    if (!parent_uid) g_ptr_array_add(root_tasks, g_object_ref(item));
  }
  // Single pass to build parent-child relationships using hash table lookup
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, self->tasks_items);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    GObject *child_item = value;
    TaskData *child_td = g_object_get_data(child_item, "data");
    const char *parent_uid = errands_data_get_str(child_td, DATA_PROP_PARENT);
    if (parent_uid) {
      GObject *parent_item = g_hash_table_lookup(self->tasks_items, parent_uid);
      if (parent_item) {
        GListStore *parent_children = g_object_get_data(parent_item, "children");
        g_list_store_append(parent_children, child_item);
      }
    }
  }
  // Add root tasks to base model
  for (size_t i = 0; i < root_tasks->len; i++) g_list_store_append(self->base_model, g_ptr_array_index(root_tasks, i));
  // Cleanup
  g_ptr_array_free(tasks, false);
  g_ptr_array_free(root_tasks, true);
  tb_log("Task List: Loaded %zu tasks into model", len);
}

// void errands_task_list_load_tasks(ErrandsTaskList *self) {
//   tb_log("Task List: Loading tasks into model");
//   TB_PROFILE_FUNC_START;
//   self->tasks_items = g_hash_table_new_full(NULL, NULL, g_free, g_object_unref);
//   GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
//   size_t len = tasks->len;
//   for_range(i, 0, len) {
//     TaskData *td = tasks->pdata[i];
//     const char *task_uid = errands_data_get_str(td, DATA_PROP_UID);
//     GObject *item = task_data_as_gobject(td);
//     g_hash_table_insert(self->tasks_items, g_strdup(task_uid), item);
//     const char *parent_uid = errands_data_get_str(td, DATA_PROP_PARENT);
//     if (!parent_uid) g_list_store_append(self->base_model, item);
//   }
//   g_ptr_array_free(tasks, false);
//   tasks = g_hash_table_get_values_as_ptr_array(self->tasks_items);
//   for_range(i, 0, tasks->len) {
//     GObject *item = tasks->pdata[i];
//     TaskData *td = g_object_get_data(item, "data");
//     const char *uid = errands_data_get_str(td, DATA_PROP_UID);
//     GListStore *children = g_object_get_data(item, "children");
//     for_range(j, 0, tasks->len) {
//       GObject *child = tasks->pdata[j];
//       TaskData *td = g_object_get_data(child, "data");
//       const char *parent = errands_data_get_str(td, DATA_PROP_PARENT);
//       if (parent && g_str_equal(parent, uid)) g_list_store_append(children, child);
//     }
//   }
//   g_ptr_array_free(tasks, false);
//   TB_PROFILE_FUNC_END;
//   tb_log("Task List: Loaded %zu tasks into model", len);
// }

void errands_task_list_update_title(ErrandsTaskList *self) {
  // Initialize completed and total counters
  size_t completed = 0, total = 0;
  const char *name = NULL;
  const char *list_uid = NULL;
  switch (self->page) {
  case ERRANDS_TASK_LIST_PAGE_ALL: adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("All Tasks")); break;
  case ERRANDS_TASK_LIST_PAGE_TODAY: adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("Today Tasks")); break;
  case ERRANDS_TASK_LIST_PAGE_TRASH: break;
  case ERRANDS_TASK_LIST_PAGE_TASK_LIST: {
    name = errands_data_get_str(self->data, DATA_PROP_LIST_NAME);
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), name);
    list_uid = errands_data_get_str(self->data, DATA_PROP_LIST_UID);
  } break;
  }
  // Retrieve tasks and count completed and total tasks
  GPtrArray *tasks = reload_all_tasks(self);
  for_range(i, 0, tasks->len) {
    TaskData *td = tasks->pdata[i];
    bool deleted = errands_data_get_bool(td, DATA_PROP_DELETED);
    bool trash = errands_data_get_bool(td, DATA_PROP_TRASH);
    const char *task_list_uid = errands_data_get_str(td, DATA_PROP_LIST_UID);
    // Count tasks based on conditions
    if (!deleted && !trash && (!list_uid || g_str_equal(task_list_uid, list_uid))) {
      total++;
      if (!icaltime_is_null_time(errands_data_get_time(td, DATA_PROP_COMPLETED_TIME))) completed++;
    }
  }
  // Set subtitle with completed stats
  const char *stats = tb_tmp_str_printf("%s %zu / %zu", _("Completed:"), completed, total);
  adw_window_title_set_subtitle(ADW_WINDOW_TITLE(self->title), total > 0 ? stats : "");
}

void errands_task_list_show_today_tasks(ErrandsTaskList *self) {
  // errands_task_list_show_all_tasks(self);
  self->page = ERRANDS_TASK_LIST_PAGE_TODAY;
  // gtk_filter_changed(GTK_FILTER(self->today_filter), GTK_FILTER_CHANGE_MORE_STRICT);
  // expand_rows(self, EXPAND_DUE);
  errands_task_list_update_title(self);
}

static void show_all_tasks_cb(ErrandsTaskList *self) {
  gtk_filter_changed(GTK_FILTER(self->master_filter), GTK_FILTER_CHANGE_LESS_STRICT);
  gtk_widget_set_sensitive(GTK_WIDGET(self), true);
  errands_task_list_update_title(self);
  if (g_list_model_get_n_items(G_LIST_MODEL(self->master_filter_model)) > 0)
    gtk_list_view_scroll_to(GTK_LIST_VIEW(self->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
}

void errands_task_list_show_all_tasks(ErrandsTaskList *self) {
  gtk_widget_set_sensitive(GTK_WIDGET(self), false);
  gtk_revealer_set_reveal_child(GTK_REVEALER(self->entry_rev), false);
  self->page = ERRANDS_TASK_LIST_PAGE_ALL;
  g_idle_add_once((GSourceOnceFunc)show_all_tasks_cb, self);
}

static void show_task_list_cb(ErrandsTaskList *self) {
  gtk_filter_changed(GTK_FILTER(self->master_filter), GTK_FILTER_CHANGE_DIFFERENT);
  gtk_widget_set_sensitive(GTK_WIDGET(self), true);
  errands_task_list_update_title(self);
  if (g_list_model_get_n_items(G_LIST_MODEL(self->master_filter_model)) > 0)
    gtk_list_view_scroll_to(GTK_LIST_VIEW(self->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
}

void errands_task_list_show_task_list(ErrandsTaskList *self, ListData *data) {
  gtk_widget_set_sensitive(GTK_WIDGET(self), false);
  // if (self->page != ERRANDS_TASK_LIST_PAGE_TASK_LIST) restore_expanded_rows(self);
  self->data = data;
  // Filter task list
  self->page = ERRANDS_TASK_LIST_PAGE_TASK_LIST;
  // Show entry
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->search_btn)))
    gtk_revealer_set_reveal_child(GTK_REVEALER(self->entry_rev), true);
  g_idle_add_once((GSourceOnceFunc)show_task_list_cb, self);
}

// ---------- PRIVATE FUNCTIONS ---------- //

static GPtrArray *reload_all_tasks(ErrandsTaskList *self) {
  if (self->all_tasks) g_ptr_array_free(self->all_tasks, false);
  self->all_tasks = g_hash_table_get_values_as_ptr_array(tdata);
  return self->all_tasks;
}

static gboolean task_matches_search_query(TaskData *data, const char *query) {
  if (!query || g_str_equal(query, "")) return true;
  const char *text = errands_data_get_str(data, DATA_PROP_TEXT);
  const char *notes = errands_data_get_str(data, DATA_PROP_NOTES);
  g_auto(GStrv) tags = errands_data_get_strv(data, DATA_PROP_TAGS);
  if ((text && tb_str_contains_case(text, query)) || (notes && tb_str_contains_case(notes, query)) ||
      (tags && g_strv_contains((const gchar *const *)tags, query)))
    return true;
  return false;
}

static gboolean task_or_descendants_match_search_query(TaskData *data, const char *query, GPtrArray *all_tasks) {
  if (task_matches_search_query(data, query)) return true;
  const char *uid = errands_data_get_str(data, DATA_PROP_UID);
  for_range(i, 0, all_tasks->len) {
    TaskData *child = all_tasks->pdata[i];
    const char *parent = errands_data_get_str(child, DATA_PROP_PARENT);
    if (parent && g_str_equal(parent, uid))
      if (task_or_descendants_match_search_query(child, query, all_tasks)) return true;
  }

  return false;
}

static bool task_is_due(TaskData *data) {
  icaltimetype due = errands_data_get_time(data, DATA_PROP_DUE_TIME);
  if (!icaltime_is_null_time(due))
    if (icaltime_compare(due, icaltime_today()) <= 0) return true;

  return false;
}

static bool task_or_descendants_is_due(TaskData *data, GPtrArray *all_tasks) {
  if (task_is_due(data)) return true;
  const char *uid = errands_data_get_str(data, DATA_PROP_UID);
  for_range(i, 0, all_tasks->len) {
    TaskData *child = all_tasks->pdata[i];
    const char *parent = errands_data_get_str(child, DATA_PROP_PARENT);
    if (parent && g_str_equal(parent, uid))
      if (task_or_descendants_is_due(child, all_tasks)) return true;
  }

  return false;
}

static void expand_rows(ErrandsTaskList *self, ExpandType type) {
  guint n = g_list_model_get_n_items(G_LIST_MODEL(self->search_filter_model));
  GPtrArray *all_tasks = reload_all_tasks(self);
  for_range(i, 0, n) {
    g_autoptr(GtkTreeListRow) row = g_list_model_get_item(G_LIST_MODEL(self->search_filter_model), i);
    if (!row) continue;
    g_autoptr(GObject) row_item = gtk_tree_list_row_get_item(row);
    TaskData *data = g_object_get_data(row_item, "data");
    switch (type) {
    case EXPAND_DUE: {
      if (data && task_or_descendants_is_due(data, all_tasks)) {
        gtk_tree_list_row_set_expanded(row, true);
        GtkListItem *list_item = g_object_get_data(G_OBJECT(row), "list-item");
        if (!list_item) continue;
        gtk_list_item_set_activatable(list_item, false);
        ErrandsTask *task = g_object_get_data(row_item, "task");
        gtk_widget_set_visible(task->sub_entry, false);
      }
    } break;
    case EXPAND_SEARCH: {
      if (data && task_or_descendants_match_search_query(data, self->search_query, all_tasks)) {
        gtk_tree_list_row_set_expanded(row, true);
        GtkListItem *list_item = g_object_get_data(G_OBJECT(row), "list-item");
        if (!list_item) continue;
        gtk_list_item_set_activatable(list_item, false);
        ErrandsTask *task = g_object_get_data(row_item, "task");
        gtk_widget_set_visible(task->sub_entry, false);
      }
    } break;
    }
  }
  if (n > 0) gtk_list_view_scroll_to(GTK_LIST_VIEW(self->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
}

static void restore_expanded_rows(ErrandsTaskList *self) {
  guint n = g_list_model_get_n_items(G_LIST_MODEL(self->search_filter_model));
  for_range(i, 0, n) {
    g_autoptr(GtkTreeListRow) row = g_list_model_get_item(G_LIST_MODEL(self->search_filter_model), i);
    if (!row) continue;
    g_autoptr(GObject) row_item = gtk_tree_list_row_get_item(row);
    TaskData *data = g_object_get_data(row_item, "data");
    if (!data) continue;
    bool expanded = errands_data_get_bool(data, DATA_PROP_EXPANDED);
    gtk_tree_list_row_set_expanded(row, expanded);
    GtkListItem *list_item = g_object_get_data(G_OBJECT(row), "list-item");
    gtk_list_item_set_activatable(list_item, true);
    if (!list_item) continue;
    ErrandsTask *task = g_object_get_data(row_item, "task");
    gtk_widget_set_visible(task->sub_entry, expanded);
  }
  if (n > 0) gtk_list_view_scroll_to(GTK_LIST_VIEW(self->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
}

// ---------- CALLBACKS ---------- //

static void on_task_list_entry_activated_cb(AdwEntryRow *entry, ErrandsTaskList *self) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  const char *list_uid = errands_data_get_str(self->data, DATA_PROP_LIST_UID);
  // Skip empty text
  if (g_str_equal(text, "")) return;
  if (g_str_equal(list_uid, "")) return;
  TaskData *td = list_data_create_task(self->data, (char *)text, list_uid, "");
  g_hash_table_insert(tdata, g_strdup(errands_data_get_str(td, DATA_PROP_UID)), td);
  g_autoptr(GObject) data_object = g_object_new(G_TYPE_OBJECT, NULL);
  g_object_set_data(data_object, "data", td);
  g_list_store_append(self->base_model, data_object);
  gtk_list_view_scroll_to(GTK_LIST_VIEW(self->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
  errands_data_write_list(self->data);
  // Clear text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  // Update counter
  errands_sidebar_task_list_row_update_counter(errands_sidebar_task_list_row_get(list_uid));
  errands_sidebar_all_row_update_counter(state.main_window->sidebar->all_row);
  errands_task_list_update_title(self);
  tb_log("Add task '%s' to task list '%s'", errands_data_get_str(td, DATA_PROP_UID),
         errands_data_get_str(td, DATA_PROP_LIST_UID));
}

static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry) {
  self->search_query = gtk_editable_get_text(GTK_EDITABLE(entry));
  gtk_filter_changed(GTK_FILTER(self->search_filter_model), GTK_FILTER_CHANGE_DIFFERENT);
  if (!self->search_query || g_str_equal(self->search_query, "")) restore_expanded_rows(self);
  else expand_rows(self, EXPAND_SEARCH);
}
