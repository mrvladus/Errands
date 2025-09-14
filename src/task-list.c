#include "task-list.h"
#include "data/data.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "settings.h"
#include "state.h"
#include "utils.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_list_view_activate(GtkListView *self, guint position, gpointer user_data);
static GListModel *create_child_model_func(gpointer item, gpointer user_data);
static void setup_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static void bind_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);

static void on_task_list_entry_activated_cb(AdwEntryRow *entry, gpointer data);
static void on_task_list_search_cb(GtkSearchEntry *entry, gpointer user_data);
static void on_sort_dialog_show_cb();
static void on_list_view_activate(GtkListView *self, guint position, gpointer user_data);

static void on_toggle_search_action_cb(GSimpleAction *action, GVariant *param, GtkToggleButton *btn);

// static bool search_filter_func(GObject *obj, gpointer user_data);
static gboolean completed_filter_func(GObject *item, gpointer user_data);
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
  // Sort model
  GtkMultiSorter *multi_sorter = gtk_multi_sorter_new();
  gtk_multi_sorter_append(multi_sorter,
                          GTK_SORTER(gtk_custom_sorter_new((GCompareDataFunc)completion_sort_func, NULL, NULL)));
  gtk_multi_sorter_append(multi_sorter, GTK_SORTER(gtk_custom_sorter_new((GCompareDataFunc)sort_func, NULL, NULL)));
  self->sorter = gtk_tree_list_row_sorter_new(GTK_SORTER(multi_sorter));
  GtkSortListModel *sort_model = gtk_sort_list_model_new(G_LIST_MODEL(self->tree_model), GTK_SORTER(self->sorter));
  // Filter model
  self->completed_filter = gtk_custom_filter_new((GtkCustomFilterFunc)completed_filter_func, NULL, NULL);
  GtkFilterListModel *completed_filter_model =
      gtk_filter_list_model_new(G_LIST_MODEL(sort_model), GTK_FILTER(self->completed_filter));

  GtkListItemFactory *tasks_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(tasks_factory, "setup", G_CALLBACK(setup_listitem_cb), NULL);
  g_signal_connect(tasks_factory, "bind", G_CALLBACK(bind_listitem_cb), NULL);

  gtk_list_view_set_model(GTK_LIST_VIEW(self->task_list),
                          GTK_SELECTION_MODEL(gtk_no_selection_new(G_LIST_MODEL(completed_filter_model))));
  gtk_list_view_set_factory(GTK_LIST_VIEW(self->task_list), tasks_factory);

  tb_log("Task List: Created");
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

static void setup_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  GtkTreeExpander *expander = g_object_new(GTK_TYPE_TREE_EXPANDER, "child", errands_task_new(), "margin-top", 3,
                                           "margin-bottom", 3, "hide-expander", true, "indent-for-icon", false, NULL);
  gtk_list_item_set_child(list_item, GTK_WIDGET(expander));
}

static void expand_row_idle_cb(GtkTreeListRow *row) { gtk_tree_list_row_set_expanded(row, true); }

static void bind_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  GtkTreeListRow *row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(list_item));
  GObject *model_item = gtk_tree_list_row_get_item(row);
  if (!model_item) return;
  // Get the expander and its child task widget
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(list_item));
  // Set the row on the expander
  gtk_tree_expander_set_list_row(expander, row); // create_child_model_func is called here
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
  if (is_expandable && expanded) g_idle_add_once((GSourceOnceFunc)expand_row_idle_cb, row);
  // gtk_filter_changed(GTK_FILTER(state.main_window->task_list->completed_filter), GTK_FILTER_CHANGE_DIFFERENT);
}

// Expand task on click on row
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
  ErrandsTask *task = g_object_get_data(item, "task");
  errands_task_set_data(task, task_data);
  errands_data_write_list(task_data_get_list(task_data));
}

static GListModel *create_child_model_func(gpointer item, gpointer user_data) {
  // Check if we already created and cached a child model
  GListModel *cached = g_object_get_data(G_OBJECT(item), "children-model");
  if (cached) return g_object_ref(cached);
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
      g_autoptr(GObject) obj = task_data_as_gobject(task);
      g_list_store_append(children_store, obj);
      children_n++;
    }
  }
  g_ptr_array_free(all_tasks, false);
  // Cache the model on the item
  g_object_set_data_full(G_OBJECT(item), "children-model", children_store, g_object_unref);
  return G_LIST_MODEL(g_object_ref(children_store));
}

// --- SORT AND FILTER CALLBACKS --- //

static gint completion_sort_func(gconstpointer a, gconstpointer b, gpointer user_data) {
  TaskData *data_a = g_object_get_data(G_OBJECT(a), "data");
  TaskData *data_b = g_object_get_data(G_OBJECT(b), "data");
  icaltimetype completed_a_t = errands_data_get_time(data_a, DATA_PROP_COMPLETED_TIME);
  icaltimetype completed_b_t = errands_data_get_time(data_b, DATA_PROP_COMPLETED_TIME);
  bool completed_a = !icaltime_is_null_date(completed_a_t);
  bool completed_b = !icaltime_is_null_date(completed_b_t);

  return completed_a - completed_b;
}

static gint sort_func(gconstpointer a, gconstpointer b, gpointer user_data) {
  TaskData *data_a = g_object_get_data(G_OBJECT(a), "data");
  TaskData *data_b = g_object_get_data(G_OBJECT(b), "data");
  switch (errands_settings_get("sort_by", SETTING_TYPE_INT).i) {
  case SORT_TYPE_CREATION_DATE: {
    icaltimetype creation_date_a = errands_data_get_time(data_a, DATA_PROP_CREATED_TIME);
    icaltimetype creation_date_b = errands_data_get_time(data_b, DATA_PROP_CREATED_TIME);

    return icaltime_compare(creation_date_b, creation_date_a);
  }
  case SORT_TYPE_DUE_DATE: {
    icaltimetype due_date_a = errands_data_get_time(data_a, DATA_PROP_DUE_TIME);
    icaltimetype due_date_b = errands_data_get_time(data_b, DATA_PROP_DUE_TIME);
    bool due_date_a_null = icaltime_is_null_time(due_date_a);
    bool due_date_b_null = icaltime_is_null_time(due_date_b);

    return due_date_b_null - due_date_a_null;
  }
  case SORT_TYPE_PRIORITY: {
    uint8_t p_a = errands_data_get_int(data_a, DATA_PROP_PRIORITY);
    uint8_t p_b = errands_data_get_int(data_b, DATA_PROP_PRIORITY);

    return p_b - p_a;
  }
  default: return 0;
  }
}

static gboolean completed_filter_func(GObject *item, gpointer user_data) {
  bool show_completed = errands_settings_get("show_completed", SETTING_TYPE_BOOL).b;
  TaskData *data = g_object_get_data(G_OBJECT(gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item))), "data");
  bool is_completed = !icaltime_is_null_date(errands_data_get_time(data, DATA_PROP_COMPLETED_TIME));
  if (!show_completed && is_completed) return false;
  return true;
}

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
      g_autoptr(GObject) obj = task_data_as_gobject(task);
      tb_log("Task List: Adding task '%s' %p", errands_data_get_str(task, DATA_PROP_TEXT), obj);
      g_list_store_append(self->tasks_model, obj);
    }
  }
  g_ptr_array_free(tasks, false);
  tb_log("Task List: Loaded %d top-level tasks", g_list_model_get_n_items(G_LIST_MODEL(self->tasks_model)));
}

// ---------- CALLBACKS ---------- //

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
