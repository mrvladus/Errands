#include "task-list.h"
#include "../data/data.h"
#include "../settings.h"
#include "../state.h"
#include "../task/task.h"
#include "../utils.h"

#include <glib/gi18n.h>

static void on_task_added(AdwEntryRow *entry, gpointer data);
static void on_task_list_search(GtkSearchEntry *entry, gpointer user_data);
static void on_search_btn_toggle(GtkToggleButton *btn);
static void on_sort_btn_clicked();

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

  GtkWidget *sort_btn =
      g_object_new(GTK_TYPE_BUTTON, "icon-name", "errands-sort-symbolic", "tooltip-text", _("Filter and Sort"), NULL);
  g_signal_connect(sort_btn, "clicked", G_CALLBACK(on_sort_btn_clicked), NULL);
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
  GtkWidget *task_entry = g_object_new(ADW_TYPE_ENTRY_ROW, "title", _("Add Task"), "activatable", false, NULL);
  gtk_widget_add_css_class(task_entry, "card");
  g_signal_connect(task_entry, "entry-activated", G_CALLBACK(on_task_added), NULL);
  self->entry = g_object_new(GTK_TYPE_REVEALER, "transition-type", GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN, "child",
                             g_object_new(ADW_TYPE_CLAMP, "child", task_entry, "tightening-threshold", 300,
                                          "maximum-size", 1000, "margin-top", 6, "margin-bottom", 6, NULL),
                             NULL);

  // Tasks Box
  self->task_list = gtk_list_box_new();
  gtk_widget_add_css_class(self->task_list, "background");
  g_object_set(self->task_list, "margin-bottom", 18, "margin-top", 6, "selection-mode", GTK_SELECTION_NONE, "vexpand",
               true, NULL);
  gtk_widget_add_css_class(self->task_list, "boxed-list-separate");
  LOG("Task List: Loading tasks");
  GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  for (size_t i = 0; i < tasks->len; i++) {
    TaskData *data = tasks->pdata[i];
    const char *parent = errands_data_get_str(data, DATA_PROP_PARENT);
    bool deleted = errands_data_get_bool(data, DATA_PROP_DELETED);
    if (!parent && !deleted) {
      GtkWidget *task = GTK_WIDGET(errands_task_new(data));
      gtk_list_box_append(GTK_LIST_BOX(self->task_list), task);
    }
  }
  g_ptr_array_free(tasks, false);
  LOG("Task List: Loading tasks complete");

  // Task list clamp
  GtkWidget *tbox_clamp =
      g_object_new(ADW_TYPE_CLAMP, "child", self->task_list, "tightening-threshold", 300, "maximum-size", 1000, NULL);

  // Scrolled window
  GtkWidget *scrl = g_object_new(GTK_TYPE_SCROLLED_WINDOW, "child", tbox_clamp, "propagate-natural-height", true,
                                 "propagate-natural-width", true, NULL);

  // VBox
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(vbox), self->entry);
  gtk_box_append(GTK_BOX(vbox), scrl);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), vbox);

  // Create task list page in the view stack
  adw_view_stack_add_named(ADW_VIEW_STACK(state.main_window->stack), GTK_WIDGET(self), "errands_task_list_page");

  errands_task_list_sort_recursive(self->task_list);
  LOG("Task List: Created");
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

void errands_task_list_add(TaskData *td) {
  gtk_box_append(GTK_BOX(state.task_list->task_list), GTK_WIDGET(errands_task_new(td)));
}

void errands_task_list_update_title() {
  // If no data - show for all tasks
  if (!state.task_list->data) {
    adw_window_title_set_title(ADW_WINDOW_TITLE(state.task_list->title), _("All Tasks"));
    // Set completed stats
    size_t completed = 0;
    size_t total = 0;
    GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
    for (size_t i = 0; i < tasks->len; i++) {
      TaskData *td = tasks->pdata[i];
      if (!errands_data_get_bool(td, DATA_PROP_DELETED) && !errands_data_get_bool(td, DATA_PROP_TRASH)) {
        total++;
        if (errands_data_get_str(td, DATA_PROP_COMPLETED)) completed++;
      }
    }
    g_ptr_array_free(tasks, false);
    g_autofree char *stats = g_strdup_printf("%s %zu / %zu", _("Completed:"), completed, total);
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.task_list->title), total > 0 ? stats : "");
    return;
  }

  // Set name of the list
  const char *name = errands_data_get_str(state.task_list->data, DATA_PROP_LIST_NAME);
  adw_window_title_set_title(ADW_WINDOW_TITLE(state.task_list->title), name);

  // Set completed stats
  size_t completed = 0;
  size_t total = 0;
  const char *list_uid = errands_data_get_str(state.task_list->data, DATA_PROP_LIST_UID);
  GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  for (size_t i = 0; i < tasks->len; i++) {
    TaskData *td = tasks->pdata[i];
    bool deleted = errands_data_get_bool(td, DATA_PROP_DELETED);
    bool trash = errands_data_get_bool(td, DATA_PROP_TRASH);
    const char *task_list_uid = errands_data_get_str(td, DATA_PROP_LIST_UID);
    if (g_str_equal(task_list_uid, list_uid) && !deleted && !trash) {
      total++;
      if (errands_data_get_str(td, DATA_PROP_COMPLETED)) completed++;
    }
  }
  g_ptr_array_free(tasks, false);
  g_autofree gchar *stats = g_strdup_printf("%s %zu / %zu", _("Completed:"), completed, total);
  adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.task_list->title), total > 0 ? stats : "");
  // errands_data_write_list(state.task_list->data);
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

void errands_task_list_filter_by_completion(GtkWidget *task_list, bool show_completed) {
  GPtrArray *tasks = get_children(task_list);
  for (size_t i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    bool deleted = errands_data_get_bool(task->data, DATA_PROP_DELETED);
    bool trash = errands_data_get_bool(task->data, DATA_PROP_TRASH);
    bool completed = errands_data_get_str(task->data, DATA_PROP_COMPLETED);
    // gtk_revealer_set_reveal_child(GTK_REVEALER(task->revealer), !deleted && !trash && (!completed ||
    // show_completed));
    errands_task_list_filter_by_completion(task->sub_tasks, show_completed);
  }
}

void errands_task_list_filter_by_uid(const char *uid) {
  GPtrArray *tasks = get_children(state.task_list->task_list);
  for (size_t i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    gtk_widget_set_visible(GTK_WIDGET(task), true);
    bool deleted = errands_data_get_bool(task->data, DATA_PROP_DELETED);
    bool trash = errands_data_get_bool(task->data, DATA_PROP_TRASH);
    const char *list_uid = errands_data_get_str(task->data, DATA_PROP_LIST_UID);
    if (g_str_equal(uid, "") && !deleted && !trash) {
      gtk_widget_set_visible(GTK_WIDGET(task), true);
      continue;
    }
    if (g_str_equal(uid, list_uid) && !deleted && !trash) gtk_widget_set_visible(GTK_WIDGET(task), true);
    else gtk_widget_set_visible(GTK_WIDGET(task), g_str_equal(list_uid, uid) && !deleted);
  }
}

bool errands_task_list_filter_by_text(GtkWidget *task_list, const char *text) {
  bool search_all_tasks = !state.task_list->data;
  GPtrArray *tasks = get_children(task_list);
  bool res = false;
  for (size_t i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    const char *list_uid = errands_data_get_str(task->data, DATA_PROP_LIST_UID);
    if ((!search_all_tasks && strcmp(errands_data_get_str(state.task_list->data, DATA_PROP_LIST_UID), list_uid)) ||
        errands_data_get_bool(task->data, DATA_PROP_DELETED) || errands_data_get_bool(task->data, DATA_PROP_TRASH))
      continue;
    const char *task_text = errands_data_get_str(task->data, DATA_PROP_TEXT);
    const char *task_notes = errands_data_get_str(task->data, DATA_PROP_NOTES);
    GStrv tags = errands_data_get_strv(task->data, DATA_PROP_TAGS);
    bool contains = (task_text && string_contains(task_text, text)) ||
                    (task_notes && string_contains(task_notes, text)) ||
                    (tags && g_strv_contains((const gchar *const *)tags, text));
    bool sub_tasks_contains = errands_task_list_filter_by_text(task->sub_tasks, text);
    if (contains || sub_tasks_contains) {
      res = true;
      gtk_widget_set_visible(GTK_WIDGET(task), true);
    } else {
      gtk_widget_set_visible(GTK_WIDGET(task), false);
    }
  }
  return res;
}

static bool errands_task_list_sorted_by_completion(GtkWidget *task_list) {
  ErrandsTask *task = (ErrandsTask *)gtk_widget_get_last_child(task_list);
  ErrandsTask *prev_task = NULL;
  while (task) {
    prev_task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
    if (prev_task)
      if ((bool)errands_data_get_str(task->data, DATA_PROP_COMPLETED) -
              (bool)errands_data_get_str(prev_task->data, DATA_PROP_COMPLETED) <=
          0)
        return false;
    task = prev_task;
  }
  return true;
}

void errands_task_list_sort_by_completion(GtkWidget *task_list) {
  // Check if the task list is already sorted by completion
  if (errands_task_list_sorted_by_completion(task_list)) return;
  ErrandsTask *task = (ErrandsTask *)gtk_widget_get_last_child(task_list);
  // Return if there are no tasks
  if (!task) return;
  // Skip if last task completed
  ErrandsTask *last_cmp_task = task;
  if (errands_data_get_str(task->data, DATA_PROP_COMPLETED))
    task = last_cmp_task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
  // Sort the rest of the tasks
  while (task) {
    if (errands_data_get_str(task->data, DATA_PROP_COMPLETED)) {
      gtk_box_reorder_child_after(GTK_BOX(task_list), GTK_WIDGET(task), GTK_WIDGET(last_cmp_task));
      last_cmp_task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
    }
    task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
  }
}

// - SORT BY DUE DATE - //

static gint __due_date_sort_func(gconstpointer a, gconstpointer b) {
  // ErrandsTask *task_a = *((ErrandsTask **)a);
  // ErrandsTask *task_b = *((ErrandsTask **)b);
  // g_autoptr(GDateTime) dt_a = parse_date(task_a->data->due_date);
  // g_autoptr(GDateTime) dt_b = parse_date(task_b->data->due_date);
  // if (!dt_a && dt_b)
  //   return -1;
  // else if (dt_a && !dt_b)
  //   return 1;
  // else if (!dt_a && !dt_b)
  //   return 0;
  // return g_date_time_compare(dt_a, dt_b);
}

void errands_task_list_sort_by_due_date(GtkWidget *task_list) {
  GPtrArray *children = get_children(task_list);
  if (children->len <= 1) return;
  g_ptr_array_sort(children, __due_date_sort_func);
  for (int i = 0; i < children->len; i++) {
    GtkWidget *task = children->pdata[i];
    GtkWidget *first_task = gtk_widget_get_first_child(task_list);
    if (task != first_task) gtk_widget_insert_before(task, task_list, first_task);
  }
}

// - SORT BY PRIORITY - //

static gint priority_sort_func(gconstpointer a, gconstpointer b) {
  size_t p1 = errands_data_get_int(((ErrandsTask *)a)->data, DATA_PROP_PRIORITY);
  size_t p2 = errands_data_get_int(((ErrandsTask *)b)->data, DATA_PROP_PRIORITY);
  return p1 > p2;
}

void errands_task_list_sort_by_priority(GtkWidget *task_list) {
  GPtrArray *children = get_children(task_list);
  g_ptr_array_sort_values(children, priority_sort_func);
  for (int i = 0; i < children->len; i++) {
    GtkWidget *task = children->pdata[i];
    GtkWidget *first_task = gtk_widget_get_first_child(task_list);
    if (task != first_task) gtk_widget_insert_before(task, task_list, first_task);
  }
}

// - SORT BY CREATION DATE - //

static gint creation_date_sort_func(gconstpointer a, gconstpointer b) {
  const char *date_a = errands_data_get_str(((ErrandsTask *)a)->data, DATA_PROP_CREATED);
  const char *date_b = errands_data_get_str(((ErrandsTask *)b)->data, DATA_PROP_CREATED);
  g_autoptr(GDateTime) dt_a = parse_date(date_a);
  g_autoptr(GDateTime) dt_b = parse_date(date_b);
  if (!dt_a && dt_b) return -1;
  else if (dt_a && !dt_b) return 1;
  else if (!dt_a && !dt_b) return 0;
  return g_date_time_compare(dt_a, dt_b);
}

void errands_task_list_sort_by_creation_date(GtkWidget *task_list) {
  GPtrArray *children = get_children(task_list);
  if (children->len <= 1) return;
  LOG("Task List: Sort by creation date");
  g_ptr_array_sort_values(children, creation_date_sort_func);
  for (size_t i = 0; i < children->len; i++) {
    GtkWidget *task = children->pdata[i];
    GtkWidget *first_task = gtk_widget_get_first_child(task_list);
    if (task != first_task) gtk_widget_insert_before(task, task_list, first_task);
  }
  g_ptr_array_free(children, false);
}

// - ALL SORT - //

void errands_task_list_sort(GtkWidget *task_list) {
  const char *sort_by = errands_settings_get("sort_by", SETTING_TYPE_STRING).s;
  // if (g_str_equal(sort_by, "created")) errands_task_list_sort_by_creation_date(task_list);
  // else if (g_str_equal(sort_by, "due")) errands_task_list_sort_by_due_date(task_list);
  // else if (g_str_equal(sort_by, "priority")) errands_task_list_sort_by_priority(task_list);
  // errands_task_list_sort_by_completion(task_list);
}

void errands_task_list_sort_recursive(GtkWidget *task_list) {
  LOG("Task List: Sort recursive");
  errands_task_list_sort(task_list);
  GPtrArray *children = get_children(task_list);
  for (int i = 0; i < children->len; i++) {
    ErrandsTask *task = children->pdata[i];
    errands_task_list_sort(task->sub_tasks);
  }
}

void errands_task_list_reload() {
  LOG("Task List: Reload");
  gtk_box_remove_all(state.task_list->task_list);
  GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  for (size_t i = 0; i < tasks->len; i++) {
    TaskData *data = tasks->pdata[i];
    if (g_str_equal(errands_data_get_str(data, DATA_PROP_PARENT), "") &&
        !errands_data_get_bool(data, DATA_PROP_DELETED))
      gtk_box_append(GTK_BOX(state.task_list->task_list), GTK_WIDGET(errands_task_new(data)));
  }
  g_ptr_array_free(tasks, false);
  errands_task_list_sort_recursive(state.task_list->task_list);
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
  errands_data_write_list(state.task_list->data);
  ErrandsTask *t = errands_task_new(td);
  gtk_list_box_prepend(GTK_LIST_BOX(state.task_list->task_list), GTK_WIDGET(t));
  // Sort list
  errands_task_list_sort(state.task_list->task_list);

  // Clear text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");

  // Update counter
  // errands_sidebar_task_list_row_update_counter(errands_sidebar_task_list_row_get(list_uid));
  // errands_sidebar_all_row_update_counter(state.sidebar->all_row);

  errands_task_list_update_title();

  LOG("Add task '%s' to task list '%s'", errands_data_get_str(td, DATA_PROP_UID),
      errands_data_get_str(td, DATA_PROP_LIST_UID));
}

static void on_task_list_search(GtkSearchEntry *entry, gpointer user_data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  errands_task_list_filter_by_text(state.task_list->task_list, text);
}

static void on_search_btn_toggle(GtkToggleButton *btn) {
  bool active = gtk_toggle_button_get_active(btn);
  LOG("Task List: Toggle search %s", active ? "on" : "off");
  if (state.task_list->data) gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), !active);
  else gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), false);
}

static void on_sort_btn_clicked() { errands_sort_dialog_show(); }
