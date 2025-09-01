#include "data/data.h"
#include "state.h"
#include "utils.h"
#include "widgets.h"

// #define TB_ENABLE_PROFILING
#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libical/ical.h>
#include <stdbool.h>
#include <stddef.h>

static void on_list_view_activate(GtkListView *self, guint position, gpointer user_data);
static GListModel *create_child_model_func(gpointer item, gpointer user_data);

static void on_task_list_entry_activated_cb(AdwEntryRow *entry, gpointer data);
static void on_task_list_search_cb(GtkSearchEntry *entry, gpointer user_data);
static void on_sort_dialog_show_cb();
static void on_test_btn_clicked_cb();
static void on_list_view_activate(GtkListView *self, guint position, gpointer user_data);

static void on_toggle_search_action_cb(GSimpleAction *action, GVariant *param, GtkToggleButton *btn);

static bool toplevel_tasks_filter_func(GObject *obj, gpointer user_data);
// static bool search_filter_func(GObject *obj, gpointer user_data);
static gint sort_func(gconstpointer a, gconstpointer b, gpointer user_data);
static gint completion_sort_func(gconstpointer a, gconstpointer b, gpointer user_data);

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
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_test_btn_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_sort_dialog_show_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_search_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_list_view_activate);
}

static void errands_task_list_init(ErrandsTaskList *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
  errands_add_actions(GTK_WIDGET(self), "task-list", "toggle_search", on_toggle_search_action_cb, NULL, NULL);

  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(self->search_bar), GTK_EDITABLE(self->search_entry));

  // Create main items model
  self->tasks_model = g_list_store_new(G_TYPE_OBJECT);
  // Create tree list model
  self->tree_model =
      gtk_tree_list_model_new(G_LIST_MODEL(self->tasks_model), false, false, create_child_model_func, NULL, NULL);

  GtkListItemFactory *tasks_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(tasks_factory, "setup", G_CALLBACK(setup_listitem_cb), NULL);
  g_signal_connect(tasks_factory, "bind", G_CALLBACK(bind_listitem_cb), NULL);

  gtk_list_view_set_model(GTK_LIST_VIEW(self->task_list),
                          GTK_SELECTION_MODEL(gtk_no_selection_new(G_LIST_MODEL(self->tree_model))));
  gtk_list_view_set_factory(GTK_LIST_VIEW(self->task_list), tasks_factory);

  tb_log("Task List: Created");
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

void setup_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  GtkTreeExpander *expander =
      g_object_new(GTK_TYPE_TREE_EXPANDER, "margin-top", 3, "margin-bottom", 3, "focusable", false, NULL);
  ErrandsTask *task = errands_task_new();
  gtk_tree_expander_set_child(expander, GTK_WIDGET(task));
  gtk_list_item_set_child(list_item, GTK_WIDGET(expander));
}

static void expand_row_idle_cb(GtkTreeListRow *row) { gtk_tree_list_row_set_expanded(row, true); }

void bind_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  GtkTreeListRow *row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(list_item));
  GObject *item = gtk_tree_list_row_get_item(row);
  if (!item) return;
  // Get the expander and its child task widget
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(list_item));
  ErrandsTask *task = ERRANDS_TASK(gtk_tree_expander_get_child(expander));
  // Set the row on the expander
  gtk_tree_expander_set_list_row(expander, row);
  g_object_set_data(G_OBJECT(item), "task-widget", task);
  // Set the task data
  TaskData *task_data = g_object_get_data(item, "data");
  errands_task_set_data(task, task_data);
  bool expanded = errands_data_get_bool(task_data, DATA_PROP_EXPANDED);
  bool is_expandable = gtk_tree_list_row_is_expandable(row);
  // Add at idle because otherwise will srew-up all list
  if (is_expandable && expanded) g_idle_add_once((GSourceOnceFunc)expand_row_idle_cb, row);
}

static void on_list_view_activate(GtkListView *self, guint position, gpointer user_data) {
  // Get row
  GListModel *selection_model = G_LIST_MODEL(gtk_list_view_get_model(self));
  GtkTreeListRow *row = GTK_TREE_LIST_ROW(g_list_model_get_item(selection_model, position));
  if (!row) return;
  if (!gtk_tree_list_row_is_expandable(row)) return;

  // Set new expanded state
  bool expanded = !gtk_tree_list_row_get_expanded(row);
  gtk_tree_list_row_set_expanded(row, expanded);

  GObject *item = gtk_tree_list_row_get_item(row);

  TaskData *task_data = g_object_get_data(item, "data");
  errands_data_set_bool(task_data, DATA_PROP_EXPANDED, expanded);

  ErrandsTask *task = g_object_get_data(item, "task-widget");
  errands_task_set_data(task, task_data);
  errands_data_write_list(task_data_get_list(task_data));
}

static GListModel *create_child_model_func(gpointer item, gpointer user_data) {
  // Check if we already created and cached a child model
  GListModel *cached = g_object_get_data(G_OBJECT(item), "children-model");
  if (cached) {
    return g_object_ref(cached); // return new ref
  }

  TaskData *parent_data = g_object_get_data(G_OBJECT(item), "data");
  if (!parent_data) return NULL;

  const char *parent_uid = errands_data_get_str(parent_data, DATA_PROP_UID);
  if (!parent_uid) return NULL;

  // Build child store
  GListStore *children_store = g_list_store_new(G_TYPE_OBJECT);
  GPtrArray *all_tasks = g_hash_table_get_values_as_ptr_array(tdata);
  size_t children_n = 0;

  for (size_t i = 0; i < all_tasks->len; i++) {
    TaskData *task = all_tasks->pdata[i];
    const char *task_parent = errands_data_get_str(task, DATA_PROP_PARENT);

    if (task_parent && g_str_equal(task_parent, parent_uid)) {
      GObject *obj = g_object_new(G_TYPE_OBJECT, NULL);
      g_object_set_data(obj, "data", task);
      g_list_store_append(children_store, obj);
      g_object_unref(obj);
      children_n++;
    }
  }
  g_ptr_array_free(all_tasks, false);

  if (children_n == 0) {
    g_object_unref(children_store);
    return NULL;
  }

  // Cache the model on the item
  g_object_set_data_full(G_OBJECT(item), "children-model", children_store, g_object_unref);

  tb_log("Created sub-tasks model with %zu sub-tasks for %s", children_n,
         errands_data_get_str(parent_data, DATA_PROP_TEXT));

  return G_LIST_MODEL(g_object_ref(children_store));
}

// Callback to create child models
// static GListModel *create_child_model_func(gpointer item, gpointer user_data) {
//   TaskData *parent_data = g_object_get_data(G_OBJECT(item), "data");
//   if (!parent_data) return NULL;
//   const char *parent_uid = errands_data_get_str(parent_data, DATA_PROP_UID);
//   if (!parent_uid) return NULL;
//   // Find all tasks that have this parent
//   GListStore *children_store = g_list_store_new(G_TYPE_OBJECT);
//   GPtrArray *all_tasks = g_hash_table_get_values_as_ptr_array(tdata);
//   size_t children_n = 0;
//   for (size_t i = 0; i < all_tasks->len; i++) {
//     TaskData *task = all_tasks->pdata[i];
//     const char *task_parent = errands_data_get_str(task, DATA_PROP_PARENT);
//     if (task_parent && g_str_equal(task_parent, parent_uid)) {
//       GObject *obj = g_object_new(G_TYPE_OBJECT, NULL);
//       g_object_set_data(obj, "data", task);
//       g_list_store_append(children_store, obj);
//       g_object_unref(obj);
//       children_n++;
//     }
//   }
//   g_ptr_array_free(all_tasks, false);

//   // If no children found, free store and return NULL
//   if (children_n == 0) {
//     g_object_unref(children_store);
//     return NULL;
//   }

//   tb_log("Created sub-tasks model with %zu sub-tasks for %s", children_n,
//       errands_data_get_str(parent_data, DATA_PROP_TEXT));
//   return G_LIST_MODEL(children_store);
// }

// --- SORT AND FILTER CALLBACKS --- //

// static bool search_filter_func(GObject *obj, gpointer user_data) {
//   if (!state.main_window->task_list->search_query || g_str_equal(state.main_window->task_list->search_query, ""))
//     return true;

//   TaskData *data = g_object_get_data(G_OBJECT(obj), "data");
//   const char *text = errands_data_get_str(data, DATA_PROP_TEXT);
//   const char *notes = errands_data_get_str(data, DATA_PROP_NOTES);
//   g_auto(GStrv) tags = errands_data_get_strv(data, DATA_PROP_TAGS);

//   return (text && string_contains(text, state.main_window->task_list->search_query)) ||
//          (notes && string_contains(notes, state.main_window->task_list->search_query)) ||
//          (tags && g_strv_contains((const gchar *const *)tags, state.main_window->task_list->search_query));
// }

// static gint sort_func(gconstpointer a, gconstpointer b, gpointer user_data) {
//   size_t sort_by = errands_settings_get("sort_by", SETTING_TYPE_INT).i;

//   TaskData *data1 = g_object_get_data(G_OBJECT(a), "data");
//   TaskData *data2 = g_object_get_data(G_OBJECT(b), "data");

//   icaltimetype creation_date1 = errands_data_get_time(data1, DATA_PROP_CREATED_TIME);
//   icaltimetype creation_date2 = errands_data_get_time(data2, DATA_PROP_CREATED_TIME);

//   gint result = 0;

//   switch (sort_by) {
//   case SORT_TYPE_CREATION_DATE: result = icaltime_compare(creation_date2, creation_date1); break;
//   case SORT_TYPE_DUE_DATE: {
//     icaltimetype due_date1 = errands_data_get_time(data1, DATA_PROP_DUE_TIME);
//     bool due_date1_null = icaltime_is_null_time(due_date1);
//     icaltimetype due_date2 = errands_data_get_time(data2, DATA_PROP_DUE_TIME);
//     bool due_date2_null = icaltime_is_null_time(due_date2);

//     if (!due_date1_null && !due_date2_null) {
//       result = icaltime_compare(due_date2, due_date1);
//     } else if (due_date1_null && !due_date2_null) {
//       result = 1; // b has due date, a doesn't
//     } else if (!due_date1_null && due_date2_null) {
//       result = -1; // a has due date, b doesn't
//     } else {
//       result = icaltime_compare(creation_date2, creation_date1);
//     }
//     break;
//   }
//   case SORT_TYPE_PRIORITY: {
//     uint8_t p1 = errands_data_get_int(data1, DATA_PROP_PRIORITY);
//     uint8_t p2 = errands_data_get_int(data2, DATA_PROP_PRIORITY);
//     if (p2 > p1) return 1;
//     if (p2 < p1) return -1;
//     result = 0;
//     break;
//   }
//   default: result = 0;
//   }

//   // For stable sorting, use creation date as secondary sort criteria
//   if (result == 0) { result = icaltime_compare(creation_date2, creation_date1); }

//   // Final fallback: use UID for absolute stability
//   if (result == 0) {
//     const char *uid1 = errands_data_get_str(data1, DATA_PROP_UID);
//     const char *uid2 = errands_data_get_str(data2, DATA_PROP_UID);
//     result = g_strcmp0(uid1, uid2);
//   }

//   return result;
// }

// static gint completion_sort_func(gconstpointer a, gconstpointer b, gpointer user_data) {
//   TaskData *data_a = g_object_get_data(G_OBJECT(a), "data");
//   TaskData *data_b = g_object_get_data(G_OBJECT(b), "data");

//   if (!data_a || !data_b) return 0;

//   // Completion status: incomplete first, complete last
//   int completed1 = !icaltime_is_null_time(errands_data_get_time(data_a, DATA_PROP_COMPLETED_TIME)) ? 1 : 0;
//   int completed2 = !icaltime_is_null_time(errands_data_get_time(data_b, DATA_PROP_COMPLETED_TIME)) ? 1 : 0;

//   gint result = completed1 - completed2;

//   // If completion status is the same, use creation date (newest first)
//   if (result == 0) {
//     icaltimetype created1 = errands_data_get_time(data_a, DATA_PROP_CREATED_TIME);
//     icaltimetype created2 = errands_data_get_time(data_b, DATA_PROP_CREATED_TIME);
//     result = icaltime_compare(created2, created1);
//   }

//   // Final fallback: use UID for absolute stability
//   if (result == 0) {
//     const char *uid1 = errands_data_get_str(data_a, DATA_PROP_UID);
//     const char *uid2 = errands_data_get_str(data_b, DATA_PROP_UID);
//     result = g_strcmp0(uid1, uid2);
//   }

//   return result;
// }

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_load_tasks(ErrandsTaskList *self) {
  tb_log("Task List: Loading tasks to model");
  // Add only top-level tasks to the root model
  GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  for (size_t i = 0; i < tasks->len; i++) {
    TaskData *task = tasks->pdata[i];
    const char *parent = errands_data_get_str(task, DATA_PROP_PARENT);
    bool deleted = errands_data_get_bool(task, DATA_PROP_DELETED);
    // Only add non-deleted, top-level tasks to root model
    if (!parent && !deleted) {
      GObject *obj = g_object_new(G_TYPE_OBJECT, NULL);
      g_object_set_data(obj, "data", task);
      g_list_store_append(self->tasks_model, obj);
      g_object_unref(obj);
    }
  }
  g_ptr_array_free(tasks, false);
  tb_log("Task List: Loaded %d top-level tasks", g_list_model_get_n_items(G_LIST_MODEL(self->tasks_model)));
}

void errands_task_list_add_task(TaskData *task) {
  const char *parent_uid = errands_data_get_str(task, DATA_PROP_PARENT);
  if (parent_uid) {
    // Find parent item
    GObject *parent_item = g_hash_table_lookup(tdata, parent_uid);
    if (parent_item) {
      GListStore *children_store = g_object_get_data(parent_item, "children-model");
      if (!children_store) {
        children_store = g_list_store_new(G_TYPE_OBJECT);
        g_object_set_data_full(parent_item, "children-model", children_store, g_object_unref);
      }
      GObject *obj = g_object_new(G_TYPE_OBJECT, NULL);
      g_object_set_data(obj, "data", task);
      g_list_store_append(children_store, obj);
      g_object_unref(obj);
    }
  } else {
    // Top-level task → append directly to root model
    GObject *obj = g_object_new(G_TYPE_OBJECT, NULL);
    g_object_set_data(obj, "data", task);
    g_list_store_append(state.main_window->task_list->tasks_model, obj);
    g_object_unref(obj);
  }
}

void errands_task_list_remove_task(TaskData *task) {
  const char *uid = errands_data_get_str(task, DATA_PROP_UID);
  const char *parent_uid = errands_data_get_str(task, DATA_PROP_PARENT);
  GListStore *store = NULL;
  if (parent_uid) {
    GObject *parent_item = g_hash_table_lookup(tdata, parent_uid);
    if (!parent_item) return;
    store = g_object_get_data(parent_item, "children-model");
  } else store = state.main_window->task_list->tasks_model; // root list
  if (store) {
    guint n = g_list_model_get_n_items(G_LIST_MODEL(store));
    for (guint i = 0; i < n; i++) {
      GObject *child = g_list_model_get_item(G_LIST_MODEL(store), i);
      TaskData *child_data = g_object_get_data(child, "data");
      if (child_data == task) {
        g_list_store_remove(store, i);
        g_object_unref(child);
        break;
      }
      g_object_unref(child);
    }
  }
}

// ---------- CALLBACKS ---------- //

static void on_test_btn_clicked_cb() {
  char buff[10];
  for (size_t i = 0; i < 1000; ++i) {
    sprintf(buff, "task %zu", i);
    TaskData *data =
        list_data_create_task(state.main_window->task_list->data, buff,
                              errands_data_get_str(state.main_window->task_list->data, DATA_PROP_LIST_UID), "");
    GObject *data_object = g_object_new(G_TYPE_OBJECT, NULL);
    g_object_set_data(data_object, "data", data);
    g_list_store_append(state.main_window->task_list->tasks_model, data_object);
  }
  errands_sidebar_all_row_update_counter(state.main_window->sidebar->all_row);
  gtk_list_view_scroll_to(GTK_LIST_VIEW(state.main_window->task_list->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
}

static void on_sort_dialog_show_cb() { errands_task_list_sort_dialog_show(); }

void errands_task_list_update_title() {
  // Initialize completed and total counters
  size_t completed = 0, total = 0;
  const char *name;
  const char *list_uid = NULL;
  // Check if there is data for the task list
  if (!state.main_window->task_list->data) {
    adw_window_title_set_title(ADW_WINDOW_TITLE(state.main_window->task_list->title), _("All Tasks"));
  } else {
    name = errands_data_get_str(state.main_window->task_list->data, DATA_PROP_LIST_NAME);
    adw_window_title_set_title(ADW_WINDOW_TITLE(state.main_window->task_list->title), name);
    list_uid = errands_data_get_str(state.main_window->task_list->data, DATA_PROP_LIST_UID);
  }
  // Retrieve tasks and count completed and total tasks
  GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  for (size_t i = 0; i < tasks->len; i++) {
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
  g_ptr_array_free(tasks, false);
  // Set subtitle with completed stats
  const char *stats = tb_tmp_str_printf("%s %zu / %zu", _("Completed:"), completed, total);
  adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.main_window->task_list->title), total > 0 ? stats : "");
}

// --- SIGNAL HANDLERS --- //

static void on_task_list_entry_activated_cb(AdwEntryRow *entry, gpointer data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));

  const char *list_uid = errands_data_get_str(state.main_window->task_list->data, DATA_PROP_LIST_UID);
  // Skip empty text
  if (g_str_equal(text, "")) return;

  if (g_str_equal(list_uid, "")) return;

  TaskData *td = list_data_create_task(state.main_window->task_list->data, (char *)text, list_uid, "");
  g_hash_table_insert(tdata, g_strdup(errands_data_get_str(td, DATA_PROP_UID)), td);
  GObject *data_object = g_object_new(G_TYPE_OBJECT, NULL);
  g_object_set_data(data_object, "data", td);
  g_list_store_append(state.main_window->task_list->tasks_model, data_object);
  // gtk_list_view_scroll_to(GTK_LIST_VIEW(state.main_window->task_list->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
  errands_data_write_list(state.main_window->task_list->data);

  // Clear text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");

  // Update counter
  errands_sidebar_task_list_row_update_counter(errands_sidebar_task_list_row_get(list_uid));
  errands_sidebar_all_row_update_counter(state.main_window->sidebar->all_row);

  errands_task_list_update_title();

  tb_log("Add task '%s' to task list '%s'", errands_data_get_str(td, DATA_PROP_UID),
         errands_data_get_str(td, DATA_PROP_LIST_UID));
}

static void on_task_list_search_cb(GtkSearchEntry *entry, gpointer user_data) {
  // const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  // tb_log("Task List: Search '%s'", text);
  // state.main_window->task_list->search_query = text;
  // gtk_filter_changed(GTK_FILTER(state.main_window->task_list->search_filter), GTK_FILTER_CHANGE_DIFFERENT);
  // gtk_list_view_scroll_to(GTK_LIST_VIEW(state.main_window->task_list->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
}

static void on_toggle_search_action_cb(GSimpleAction *action, GVariant *param, GtkToggleButton *btn) {
  bool active = gtk_toggle_button_get_active(btn);
  tb_log("Task List: Toggle search %s", active ? "on" : "off");
  gtk_revealer_set_reveal_child(GTK_REVEALER(state.main_window->task_list->entry_rev),
                                state.main_window->task_list->data ? !active : false);
}
