#include "data/data.h"
#include "glib-object.h"
#include "glib.h"
#include "settings.h"
#include "state.h"
#include "utils.h"
#include "widgets.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_task_list_entry_activated_cb(AdwEntryRow *entry, gpointer data);
static void on_task_list_search_cb(GtkSearchEntry *entry, gpointer user_data);
static void on_sort_dialog_show_cb();
static void on_test_btn_clicked_cb();

static void on_toggle_search_action_cb(GSimpleAction *action, GVariant *param, GtkToggleButton *btn);

static void setup_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static void bind_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);

static bool toplevel_tasks_filter_func(GObject *obj, gpointer user_data);
static bool search_filter_func(GObject *obj, gpointer user_data);
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
}

static void errands_task_list_init(ErrandsTaskList *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
  errands_add_actions(GTK_WIDGET(self), "task-list", "toggle_search", on_toggle_search_action_cb, NULL, NULL);

  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(self->search_bar), GTK_EDITABLE(self->search_entry));

  // Create tasks model
  self->tasks_model = g_list_store_new(G_TYPE_OBJECT);

  self->tasks_sorter = gtk_custom_sorter_new(sort_func, NULL, NULL);
  self->sort_model = gtk_sort_list_model_new(G_LIST_MODEL(self->tasks_model), GTK_SORTER(self->tasks_sorter));

  self->completion_sorter = gtk_custom_sorter_new(completion_sort_func, NULL, NULL);
  self->completion_sort_model =
      gtk_sort_list_model_new(G_LIST_MODEL(self->sort_model), GTK_SORTER(self->completion_sorter));

  // self->search_filter = gtk_custom_filter_new((GtkCustomFilterFunc)search_filter_func, NULL, NULL);
  // self->search_filter_model =
  //     gtk_filter_list_model_new(G_LIST_MODEL(self->completion_sort_model), GTK_FILTER(self->search_filter));

  self->toplevel_tasks_filter = gtk_custom_filter_new((GtkCustomFilterFunc)toplevel_tasks_filter_func, NULL, NULL);
  self->toplevel_tasks_filter_model =
      gtk_filter_list_model_new(G_LIST_MODEL(self->completion_sort_model), GTK_FILTER(self->toplevel_tasks_filter));

  GtkListItemFactory *tasks_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(tasks_factory, "setup", G_CALLBACK(setup_listitem_cb), NULL);
  g_signal_connect(tasks_factory, "bind", G_CALLBACK(bind_listitem_cb), NULL);

  GtkNoSelection *selection_model = gtk_no_selection_new(G_LIST_MODEL(self->toplevel_tasks_filter_model));
  gtk_list_view_set_factory(GTK_LIST_VIEW(self->task_list), tasks_factory);
  gtk_list_view_set_model(GTK_LIST_VIEW(self->task_list), GTK_SELECTION_MODEL(selection_model));

  LOG("Task List: Created");
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

static void setup_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  gtk_list_item_set_child(list_item, GTK_WIDGET(errands_task_new()));
  gtk_list_item_set_activatable(list_item, false);
}

static void bind_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  ErrandsTask *task = ERRANDS_TASK(gtk_list_item_get_child(list_item));
  TaskData *data = g_object_get_data(gtk_list_item_get_item(list_item), "data");
  errands_task_set_data(task, data);
}

// --- SORT AND FILTER CALLBACKS --- //

static bool toplevel_tasks_filter_func(GObject *obj, gpointer user_data) {
  TaskData *data = g_object_get_data(G_OBJECT(obj), "data");
  const char *parent = errands_data_get_str(data, DATA_PROP_PARENT);
  const char *list_uid = errands_data_get_str(data, DATA_PROP_LIST_UID);
  bool deleted = errands_data_get_bool(data, DATA_PROP_DELETED);
  bool trash = errands_data_get_bool(data, DATA_PROP_TRASH);
  bool visible = true;
  if (state.main_window->task_list->data) {
    const char *curr_list_uid = errands_data_get_str(state.main_window->task_list->data, DATA_PROP_LIST_UID);
    visible = g_str_equal(curr_list_uid, list_uid);
  }
  return !parent && !deleted && !trash && visible;
}

static bool search_filter_func(GObject *obj, gpointer user_data) {
  if (!state.main_window->task_list->search_query || g_str_equal(state.main_window->task_list->search_query, ""))
    return true;

  TaskData *data = g_object_get_data(G_OBJECT(obj), "data");
  const char *text = errands_data_get_str(data, DATA_PROP_TEXT);
  const char *notes = errands_data_get_str(data, DATA_PROP_NOTES);
  g_auto(GStrv) tags = errands_data_get_strv(data, DATA_PROP_TAGS);

  return (text && string_contains(text, state.main_window->task_list->search_query)) ||
         (notes && string_contains(notes, state.main_window->task_list->search_query)) ||
         (tags && g_strv_contains((const gchar *const *)tags, state.main_window->task_list->search_query));
}

static gint sort_func(gconstpointer a, gconstpointer b, gpointer user_data) {
  size_t sort_by = errands_settings_get("sort_by", SETTING_TYPE_INT).i;

  TaskData *data1 = g_object_get_data(G_OBJECT(a), "data");
  TaskData *data2 = g_object_get_data(G_OBJECT(b), "data");

  icaltimetype creation_date1 = errands_data_get_time(data1, DATA_PROP_CREATED_TIME);
  icaltimetype creation_date2 = errands_data_get_time(data2, DATA_PROP_CREATED_TIME);

  switch (sort_by) {
  case SORT_TYPE_CREATION_DATE: return icaltime_compare(creation_date2, creation_date1);
  case SORT_TYPE_DUE_DATE: {
    icaltimetype due_date1 = errands_data_get_time(data1, DATA_PROP_DUE_TIME);
    bool due_date1_null = icaltime_is_null_time(due_date1);
    icaltimetype due_date2 = errands_data_get_time(data2, DATA_PROP_DUE_TIME);
    bool due_date2_null = icaltime_is_null_time(due_date2);
    if (!due_date1_null && !due_date2_null) return icaltime_compare(due_date2, due_date1);
    else return icaltime_compare(creation_date2, creation_date1);
  }
  case SORT_TYPE_PRIORITY: {
    uint8_t p1 = errands_data_get_int(data1, DATA_PROP_PRIORITY);
    uint8_t p2 = errands_data_get_int(data2, DATA_PROP_PRIORITY);
    if (p2 == p1) return 0;
    else if (p2 > p1) return 1;
    else return -1;
  }
  }

  return 0;
}

static gint completion_sort_func(gconstpointer a, gconstpointer b, gpointer user_data) {
  TaskData *data1 = g_object_get_data(G_OBJECT(a), "data");
  int completed1 = !icaltime_is_null_time(errands_data_get_time(data1, DATA_PROP_COMPLETED_TIME)) ? 1 : 0;
  TaskData *data2 = g_object_get_data(G_OBJECT(b), "data");
  int completed2 = !icaltime_is_null_time(errands_data_get_time(data2, DATA_PROP_COMPLETED_TIME)) ? 1 : 0;
  return completed1 - completed2;
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_load_tasks(ErrandsTaskList *self) {
  LOG("Task List: Loading tasks");
  GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  for (size_t i = 0; i < tasks->len; i++) {
    GObject *data_object = g_object_new(G_TYPE_OBJECT, NULL);
    g_object_set_data(data_object, "data", tasks->pdata[i]);
    g_list_store_append(self->tasks_model, data_object);
  }
  g_ptr_array_free(tasks, false);
  if (g_list_model_get_n_items(G_LIST_MODEL(self->toplevel_tasks_filter_model)) > 0) {
    gtk_list_view_scroll_to(GTK_LIST_VIEW(self->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
  }
  LOG("Task List: Loading tasks complete");
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
  g_autofree char *stats = g_strdup_printf("%s %zu / %zu", _("Completed:"), completed, total);
  adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.main_window->task_list->title), total > 0 ? stats : "");
}

static void __append_tasks(GPtrArray *arr, GtkWidget *task_list) {
  GPtrArray *tasks = get_children(task_list);
  for (size_t i = 0; i < tasks->len; i++) {
    ErrandsTask *t = tasks->pdata[i];
    g_ptr_array_add(arr, t);
    __append_tasks(arr, t->task_list);
  }
}

GPtrArray *errands_task_list_get_all_tasks() {
  GPtrArray *arr = g_ptr_array_new();
  __append_tasks(arr, state.main_window->task_list->task_list);
  return arr;
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
  // g_list_store_append(state.main_window->task_list->tasks_model, td);
  // gtk_list_view_scroll_to(GTK_LIST_VIEW(state.main_window->task_list->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
  errands_data_write_list(state.main_window->task_list->data);

  // Clear text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");

  // Update counter
  errands_sidebar_task_list_row_update_counter(errands_sidebar_task_list_row_get(list_uid));
  errands_sidebar_all_row_update_counter(state.main_window->sidebar->all_row);

  errands_task_list_update_title();

  LOG("Add task '%s' to task list '%s'", errands_data_get_str(td, DATA_PROP_UID),
      errands_data_get_str(td, DATA_PROP_LIST_UID));
}

static void on_task_list_search_cb(GtkSearchEntry *entry, gpointer user_data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  LOG("Task List: Search '%s'", text);
  state.main_window->task_list->search_query = text;
  gtk_filter_changed(GTK_FILTER(state.main_window->task_list->search_filter), GTK_FILTER_CHANGE_DIFFERENT);
  // gtk_list_view_scroll_to(GTK_LIST_VIEW(state.main_window->task_list->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
}

static void on_toggle_search_action_cb(GSimpleAction *action, GVariant *param, GtkToggleButton *btn) {
  bool active = gtk_toggle_button_get_active(btn);
  LOG("Task List: Toggle search %s", active ? "on" : "off");
  gtk_revealer_set_reveal_child(GTK_REVEALER(state.main_window->task_list->entry_rev),
                                state.main_window->task_list->data ? !active : false);
}
