#include "task-list.h"
#include "data.h"
#include "gio/gio.h"
#include "settings.h"
#include "sidebar/sidebar-all-row.h"
#include "sidebar/sidebar-task-list-row.h"
#include "state.h"
#include "task/task.h"
#include "utils.h"

#include <glib/gi18n.h>

#include <stdbool.h>
#include <string.h>

static void on_task_added(AdwEntryRow *entry, gpointer data);
static void on_task_list_search(GtkSearchEntry *entry, gpointer user_data);
static void on_search_btn_toggle(GtkToggleButton *btn);
static void on_action_toggle_completed(GSimpleAction *action, GVariant *state, gpointer user_data);

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)

static void errands_task_list_class_init(ErrandsTaskListClass *class) {}

static void errands_task_list_init(ErrandsTaskList *self) {
  LOG("Creating task list");

  // Actions
  GSimpleActionGroup *ag = g_simple_action_group_new();
  gtk_widget_insert_action_group(GTK_WIDGET(self), "task-list", G_ACTION_GROUP(ag));
  GSimpleAction *toggle_completed_action = g_simple_action_new_stateful(
      "toggle-completed", NULL,
      g_variant_new_boolean(errands_settings_get("show_completed", SETTING_TYPE_BOOL).b));
  g_signal_connect(toggle_completed_action, "change-state", G_CALLBACK(on_action_toggle_completed),
                   NULL);
  g_action_map_add_action(G_ACTION_MAP(ag), G_ACTION(toggle_completed_action));
  g_object_unref(toggle_completed_action);

  // Toolbar View
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "width-request", 360, NULL);
  adw_bin_set_child(ADW_BIN(self), tb);

  // Header Bar
  GtkWidget *hb = adw_header_bar_new();
  self->title = adw_window_title_new("", "");
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), self->title);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);

  // Menu Button
  GtkWidget *menu_btn = gtk_menu_button_new();
  g_object_set(menu_btn, "icon-name", "open-menu-symbolic", NULL);
  adw_header_bar_pack_start(ADW_HEADER_BAR(hb), menu_btn);

  // Define the menu model
  GMenu *menu = g_menu_new();
  GMenuItem *completed_item = g_menu_item_new(_("Show Completed"), "task-list.toggle-completed");
  g_menu_item_set_attribute(completed_item, "action-state", "b",
                            g_variant_new_boolean(FALSE)); // Default state is false
  g_menu_append_item(menu, completed_item);
  g_object_unref(completed_item);

  // Set the menu model on the menu button
  GMenuModel *menu_model = G_MENU_MODEL(menu);
  gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_btn), menu_model);
  g_object_unref(menu);

  // Search Bar
  GtkWidget *sb = gtk_search_bar_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), sb);
  self->search_btn = gtk_toggle_button_new();
  g_object_set(self->search_btn, "icon-name", "errands-search-symbolic", "tooltip-text",
               _("Search (Ctrl+F)"), NULL);
  gtk_widget_add_css_class(self->search_btn, "flat");
  g_object_bind_property(self->search_btn, "active", sb, "search-mode-enabled",
                         G_BINDING_BIDIRECTIONAL);
  g_signal_connect(self->search_btn, "toggled", G_CALLBACK(on_search_btn_toggle), NULL);
  errands_add_shortcut(self->search_btn, "<Control>F", "activate");
  adw_header_bar_pack_end(ADW_HEADER_BAR(hb), self->search_btn);
  GtkWidget *se = gtk_search_entry_new();
  g_signal_connect(se, "search-changed", G_CALLBACK(on_task_list_search), NULL);
  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(sb), GTK_EDITABLE(se));
  gtk_search_bar_set_key_capture_widget(GTK_SEARCH_BAR(sb), tb);
  gtk_search_bar_set_child(GTK_SEARCH_BAR(sb), se);

  // Entry
  GtkWidget *task_entry = adw_entry_row_new();
  g_object_set(task_entry, "title", _("Add Task"), "activatable", false, "margin-start", 12,
               "margin-end", 12, NULL);
  gtk_widget_add_css_class(task_entry, "card");
  g_signal_connect(task_entry, "entry-activated", G_CALLBACK(on_task_added), NULL);
  GtkWidget *entry_clamp = adw_clamp_new();
  g_object_set(entry_clamp, "child", task_entry, "tightening-threshold", 300, "maximum-size", 1000,
               "margin-top", 6, "margin-bottom", 6, NULL);
  self->entry = gtk_revealer_new();
  g_object_set(self->entry, "child", entry_clamp, "transition-type",
               GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN, "margin-start", 12, "margin-end", 12, NULL);

  // Tasks Box
  self->task_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_set(self->task_list, "margin-bottom", 18, NULL);
  LOG("Task List: Loading tasks");
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *data = state.t_data->pdata[i];
    if (!strcmp(data->parent, "") && !data->deleted)
      gtk_box_append(GTK_BOX(self->task_list), GTK_WIDGET(errands_task_new(data)));
  }

  // Task list clamp
  GtkWidget *tbox_clamp = adw_clamp_new();
  g_object_set(tbox_clamp, "tightening-threshold", 300, "maximum-size", 1000, "margin-start", 12,
               "margin-end", 12, NULL);
  adw_clamp_set_child(ADW_CLAMP(tbox_clamp), self->task_list);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  g_object_set(scrl, "child", tbox_clamp, "propagate-natural-height", true,
               "propagate-natural-width", true, NULL);

  // VBox
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(vbox), self->entry);
  gtk_box_append(GTK_BOX(vbox), scrl);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), vbox);

  // Create task list page in the view stack
  adw_view_stack_add_named(ADW_VIEW_STACK(state.main_window->stack), GTK_WIDGET(self),
                           "errands_task_list_page");

  errands_task_list_sort_by_completion(self->task_list);
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
    int completed = 0;
    int total = 0;
    for (int i = 0; i < state.t_data->len; i++) {
      TaskData *td = state.t_data->pdata[i];
      if (!td->deleted && !td->trash) {
        total++;
        if (td->completed)
          completed++;
      }
    }
    char *stats = g_strdup_printf("%s %d / %d", _("Completed:"), completed, total);
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.task_list->title), total > 0 ? stats : "");
    g_free(stats);
    return;
  }

  // Set name of the list
  adw_window_title_set_title(ADW_WINDOW_TITLE(state.task_list->title), state.task_list->data->name);

  // Set completed stats
  int completed = 0;
  int total = 0;
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!strcmp(td->list_uid, state.task_list->data->uid) && !td->deleted && !td->trash) {
      total++;
      if (td->completed)
        completed++;
    }
  }
  char *stats = g_strdup_printf("%s %d / %d", _("Completed:"), completed, total);
  adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.task_list->title), total > 0 ? stats : "");
  g_free(stats);
}

void errands_task_list_filter_by_completion(GtkWidget *task_list, bool show_completed) {
  GPtrArray *tasks = get_children(task_list);
  for (int i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    gtk_revealer_set_reveal_child(GTK_REVEALER(task->revealer),
                                  !task->data->deleted && !task->data->trash &&
                                      (!task->data->completed || show_completed));
    errands_task_list_filter_by_completion(task->sub_tasks, show_completed);
  }
  g_ptr_array_free(tasks, false);
}

void errands_task_list_filter_by_uid(const char *uid) {
  GPtrArray *tasks = get_children(state.task_list->task_list);
  for (int i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    if (!strcmp(uid, "") && !task->data->deleted && !task->data->trash) {
      gtk_widget_set_visible(GTK_WIDGET(task), true);
      continue;
    }
    if (!strcmp(uid, task->data->list_uid) && !task->data->deleted && !task->data->trash)
      gtk_widget_set_visible(GTK_WIDGET(task), true);
    else
      gtk_widget_set_visible(GTK_WIDGET(task),
                             !strcmp(task->data->list_uid, uid) && !task->data->deleted);
  }
}

void errands_task_list_filter_by_text(const char *text) {
  LOG("Task List: Filter by text '%s'", text);

  GPtrArray *tasks = get_children(state.task_list->task_list);
  bool search_all_tasks = !state.task_list->data || !strcmp(state.task_list->data->uid, "");
  // Search all tasks
  if (search_all_tasks) {
    for (int i = 0; i < tasks->len; i++) {
      ErrandsTask *task = tasks->pdata[i];
      bool contains = string_contains(task->data->text, text) ||
                      string_contains(task->data->notes, text) ||
                      string_array_contains(task->data->tags, text);
      gtk_widget_set_visible(GTK_WIDGET(task), !task->data->deleted && contains);
    }
    return;
  }
  // Search for task list uid
  for (int i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    bool contains = string_contains(task->data->text, text) ||
                    string_contains(task->data->notes, text) ||
                    string_array_contains(task->data->tags, text);
    gtk_widget_set_visible(GTK_WIDGET(task),
                           !task->data->deleted &&
                               !strcmp(task->data->list_uid, state.task_list->data->uid) &&
                               contains);
  }
  LOG("asdasd");
}

static bool errands_task_list_sorted_by_completion(GtkWidget *task_list) {
  ErrandsTask *task = (ErrandsTask *)gtk_widget_get_last_child(task_list);
  ErrandsTask *prev_task = NULL;
  while (task) {
    prev_task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
    if (prev_task)
      if (task->data->completed - prev_task->data->completed <= 0)
        return false;
    task = prev_task;
  }
  return true;
}

void errands_task_list_sort_by_completion(GtkWidget *task_list) {
  // Check if the task list is already sorted by completion
  if (errands_task_list_sorted_by_completion(task_list))
    return;

  ErrandsTask *task = (ErrandsTask *)gtk_widget_get_last_child(task_list);

  // Return if there are no tasks
  if (!task)
    return;

  // Skip if last task completed
  ErrandsTask *last_cmp_task = task;
  if (task->data->completed)
    task = last_cmp_task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));

  // Sort the rest of the tasks
  while (task) {
    if (task->data->completed) {
      gtk_box_reorder_child_after(GTK_BOX(task_list), GTK_WIDGET(task), GTK_WIDGET(last_cmp_task));
      last_cmp_task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
    }
    task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
  }
}

// --- SIGNAL HANDLERS --- //

static void on_task_added(AdwEntryRow *entry, gpointer data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));

  // Skip empty text
  if (!strcmp(text, ""))
    return;

  if (!state.task_list->data->uid)
    return;

  TaskData *td = errands_data_add_task((char *)text, state.task_list->data->uid, "");
  ErrandsTask *t = errands_task_new(td);
  gtk_box_prepend(GTK_BOX(state.task_list->task_list), GTK_WIDGET(t));

  // Clear text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");

  // Update counter
  errands_sidebar_task_list_row_update_counter(
      errands_sidebar_task_list_row_get(state.task_list->data->uid));
  errands_sidebar_all_row_update_counter(state.sidebar->all_row);

  LOG("Add task '%s' to task list '%s'", td->uid, td->list_uid);
}

static void on_task_list_search(GtkSearchEntry *entry, gpointer user_data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  errands_task_list_filter_by_text(text);
}

static void on_search_btn_toggle(GtkToggleButton *btn) {
  bool active = gtk_toggle_button_get_active(btn);
  LOG("Task List: Toggle search %s", active ? "on" : "off");
  if (state.task_list->data)
    gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), !active);
  else
    gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), false);
}

static void on_action_toggle_completed(GSimpleAction *action, GVariant *variant,
                                       gpointer user_data) {
  bool show_completed = g_variant_get_boolean(variant);
  errands_settings_set("show_completed", SETTING_TYPE_BOOL, &show_completed);
  errands_task_list_filter_by_completion(state.task_list->task_list, show_completed);
  g_simple_action_set_state(action, g_variant_new_boolean(show_completed));
  LOG("Task List: Set show completed to '%s'", show_completed ? "on" : "off");
}
