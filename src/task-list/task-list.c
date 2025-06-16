#include "task-list.h"
#include "../data/data.h"
#include "../state.h"
#include "../task/task.h"
#include "../utils.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static void on_task_added(AdwEntryRow *entry, gpointer data);
static void on_task_list_search(GtkSearchEntry *entry, gpointer user_data);
static void on_search_btn_toggle(GtkToggleButton *btn);
static void on_test_btn_clicked();

static void setup_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  gtk_list_item_set_child(list_item, GTK_WIDGET(errands_task_new()));
  gtk_list_item_set_activatable(list_item, false);
}

static void bind_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item) {
  ErrandsTask *task = ERRANDS_TASK(gtk_list_item_get_child(list_item));
  TaskData *data = g_object_get_data(gtk_list_item_get_item(list_item), "data");
  errands_task_set_data(task, data);
}

static gint sort_func_creation_date(gconstpointer a, gconstpointer b, gpointer user_data) {
  TaskData *data1 = g_object_get_data(G_OBJECT(a), "data");
  icaltimetype date1 = errands_data_get_time(data1, DATA_PROP_CREATED_TIME);
  TaskData *data2 = g_object_get_data(G_OBJECT(b), "data");
  icaltimetype date2 = errands_data_get_time(data2, DATA_PROP_CREATED_TIME);
  return icaltime_compare(date2, date1);
}

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)
static void errands_task_list_class_init(ErrandsTaskListClass *class) {}
static void errands_task_list_init(ErrandsTaskList *self) {
  LOG("Task List: Create");

  // Toolbar View
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "width-request", 360, NULL);
  adw_bin_set_child(ADW_BIN(self), tb);

  // Header Bar
  GtkWidget *hb = adw_header_bar_new();
  self->title = adw_window_title_new("", "");
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), self->title);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);

  // Test button
  GtkWidget *test_btn = g_object_new(GTK_TYPE_BUTTON, "label", "test", NULL);
  g_signal_connect(test_btn, "clicked", G_CALLBACK(on_test_btn_clicked), NULL);
  adw_header_bar_pack_start(ADW_HEADER_BAR(hb), test_btn);

  // Sort button
  GtkWidget *sort_btn =
      g_object_new(GTK_TYPE_BUTTON, "icon-name", "errands-sort-symbolic", "tooltip-text", _("Filter and Sort"), NULL);
  g_signal_connect(sort_btn, "clicked", G_CALLBACK(errands_sort_dialog_show), NULL);
  adw_header_bar_pack_start(ADW_HEADER_BAR(hb), sort_btn);

  // Search Bar
  GtkWidget *sb = gtk_search_bar_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), sb);
  self->search_btn = gtk_toggle_button_new();
  g_object_set(self->search_btn, "icon-name", "errands-search-symbolic", "tooltip-text", _("Search (Ctrl+F)"), NULL);
  gtk_widget_add_css_class(self->search_btn, "flat");
  g_object_bind_property(self->search_btn, "active", sb, "search-mode-enabled", G_BINDING_BIDIRECTIONAL);
  g_signal_connect(self->search_btn, "toggled", G_CALLBACK(on_search_btn_toggle), NULL);
  errands_add_shortcut(self->search_btn, "<Control>F", "activate");
  adw_header_bar_pack_end(ADW_HEADER_BAR(hb), self->search_btn);
  GtkWidget *se = gtk_search_entry_new();
  g_signal_connect(se, "search-changed", G_CALLBACK(on_task_list_search), NULL);
  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(sb), GTK_EDITABLE(se));
  gtk_search_bar_set_key_capture_widget(GTK_SEARCH_BAR(sb), tb);
  gtk_search_bar_set_child(GTK_SEARCH_BAR(sb), se);

  // Entry
  GtkWidget *task_entry =
      g_object_new(ADW_TYPE_ENTRY_ROW, "title", _("Add Task"), "activatable", false, "height-request", 10, NULL);
  gtk_widget_add_css_class(task_entry, "card");
  g_signal_connect(task_entry, "entry-activated", G_CALLBACK(on_task_added), NULL);
  self->entry =
      g_object_new(GTK_TYPE_REVEALER, "transition-type", GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN, "child",
                   g_object_new(ADW_TYPE_CLAMP, "child", task_entry, "tightening-threshold", 300, "maximum-size", 1000,
                                "margin-top", 6, "margin-bottom", 6, "margin-start", 11, "margin-end", 11, NULL),
                   NULL);

  LOG("Task List: Loading tasks");
  // Create tasks model
  // TODO: move to data.c
  self->tasks_model = g_list_store_new(G_TYPE_OBJECT);
  GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  for (size_t i = 0; i < tasks->len; i++) {
    TaskData *data = tasks->pdata[i];
    const char *parent = errands_data_get_str(data, DATA_PROP_PARENT);
    bool deleted = errands_data_get_bool(data, DATA_PROP_DELETED);
    if (!parent && !deleted) {
      GObject *data_object = g_object_new(G_TYPE_OBJECT, NULL);
      g_object_set_data(data_object, "data", data);
      g_list_store_append(self->tasks_model, data_object);
    }
  }
  g_ptr_array_free(tasks, false);

  GtkCustomSorter *creation_date_sorter = gtk_custom_sorter_new(sort_func_creation_date, NULL, NULL);
  GtkSortListModel *creation_date_sort_model =
      gtk_sort_list_model_new(G_LIST_MODEL(self->tasks_model), GTK_SORTER(creation_date_sorter));

  GtkNoSelection *selection_model = gtk_no_selection_new(G_LIST_MODEL(creation_date_sort_model));
  GtkListItemFactory *tasks_factory = gtk_signal_list_item_factory_new();
  g_signal_connect(tasks_factory, "setup", G_CALLBACK(setup_listitem_cb), NULL);
  g_signal_connect(tasks_factory, "bind", G_CALLBACK(bind_listitem_cb), NULL);

  self->task_list = gtk_list_view_new(GTK_SELECTION_MODEL(selection_model), tasks_factory);
  g_object_set(self->task_list, "single-click-activate", true, NULL);

  LOG("Task List: Loading tasks complete");

  // Scrolled window
  GtkWidget *scrl =
      g_object_new(GTK_TYPE_SCROLLED_WINDOW, "child", self->task_list, "propagate-natural-height", true, NULL);

  // VBox
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(vbox), self->entry);
  gtk_box_append(GTK_BOX(vbox), scrl);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), vbox);

  // Create task list page in the view stack
  adw_view_stack_add_named(ADW_VIEW_STACK(state.main_window->stack), GTK_WIDGET(self), "errands_task_list_page");

  LOG("Task List: Created");
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

static void on_test_btn_clicked() {
  char buff[10];
  for (size_t i = 0; i < 1000; ++i) {
    sprintf(buff, "task %zu", i);
    TaskData *data = list_data_create_task(state.task_list->data, buff,
                                           errands_data_get_str(state.task_list->data, DATA_PROP_LIST_UID), "");
    GObject *data_object = g_object_new(G_TYPE_OBJECT, NULL);
    g_object_set_data(data_object, "data", data);
    g_list_store_append(state.task_list->tasks_model, data_object);
  }
  errands_sidebar_all_row_update_counter(state.sidebar->all_row);
}

void errands_task_list_update_title() {
  // Initialize completed and total counters
  size_t completed = 0, total = 0;
  const char *name;
  const char *list_uid = NULL;
  // Check if there is data for the task list
  if (!state.task_list->data) {
    adw_window_title_set_title(ADW_WINDOW_TITLE(state.task_list->title), _("All Tasks"));
  } else {
    name = errands_data_get_str(state.task_list->data, DATA_PROP_LIST_NAME);
    adw_window_title_set_title(ADW_WINDOW_TITLE(state.task_list->title), name);
    list_uid = errands_data_get_str(state.task_list->data, DATA_PROP_LIST_UID);
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
      if (errands_data_get_str(td, DATA_PROP_COMPLETED)) completed++;
    }
  }
  g_ptr_array_free(tasks, false);
  // Set subtitle with completed stats
  g_autofree char *stats = g_strdup_printf("%s %zu / %zu", _("Completed:"), completed, total);
  adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.task_list->title), total > 0 ? stats : "");
}

static void __append_tasks(GPtrArray *arr, GtkWidget *task_list) {
  GPtrArray *tasks = get_children(task_list);
  for (size_t i = 0; i < tasks->len; i++) {
    ErrandsTask *t = tasks->pdata[i];
    g_ptr_array_add(arr, t);
    __append_tasks(arr, t->sub_tasks);
  }
}

GPtrArray *errands_task_list_get_all_tasks() {
  GPtrArray *arr = g_ptr_array_new();
  __append_tasks(arr, state.task_list->task_list);
  return arr;
}

// --- SIGNAL HANDLERS --- //

static void on_task_added(AdwEntryRow *entry, gpointer data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));

  const char *list_uid = errands_data_get_str(state.task_list->data, DATA_PROP_LIST_UID);
  // Skip empty text
  if (g_str_equal(text, "")) return;

  if (g_str_equal(list_uid, "")) return;

  TaskData *td = list_data_create_task(state.task_list->data, (char *)text, list_uid, "");
  g_hash_table_insert(tdata, g_strdup(errands_data_get_str(td, DATA_PROP_UID)), td);
  GObject *data_object = g_object_new(G_TYPE_OBJECT, NULL);
  g_object_set_data(data_object, "data", td);
  g_list_store_append(state.task_list->tasks_model, data_object);
  gtk_list_view_scroll_to(GTK_LIST_VIEW(state.task_list->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
  errands_data_write_list(state.task_list->data);

  // Clear text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");

  // Update counter
  errands_sidebar_task_list_row_update_counter(errands_sidebar_task_list_row_get(list_uid));
  errands_sidebar_all_row_update_counter(state.sidebar->all_row);

  errands_task_list_update_title();

  LOG("Add task '%s' to task list '%s'", errands_data_get_str(td, DATA_PROP_UID),
      errands_data_get_str(td, DATA_PROP_LIST_UID));
}

static void on_task_list_search(GtkSearchEntry *entry, gpointer user_data) {
  // const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  // errands_task_list_filter_by_text(state.task_list->task_list, text);
}

static void on_search_btn_toggle(GtkToggleButton *btn) {
  bool active = gtk_toggle_button_get_active(btn);
  LOG("Task List: Toggle search %s", active ? "on" : "off");
  if (state.task_list->data) gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), !active);
  else gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), false);
}
