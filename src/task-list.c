#include "task-list.h"
#include "data.h"
#include "gtk/gtk.h"
#include "sidebar-all-row.h"
#include "sidebar-task-list-row.h"
#include "state.h"
#include "task.h"
#include "utils.h"

#include <string.h>

static void on_task_added(AdwEntryRow *entry, gpointer data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));

  // Skip empty text
  if (!strcmp(text, ""))
    return;

  if (!state.current_uid)
    return;

  TaskData *td = errands_data_add_task((char *)text, state.current_uid, "");
  ErrandsTask *t = errands_task_new(td);
  gtk_box_prepend(GTK_BOX(state.task_list), GTK_WIDGET(t));

  // Clear text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");

  // Update counter
  errands_sidebar_task_list_row_update_counter(
      errands_sidebar_task_list_row_get(state.current_uid));
  errands_sidebar_all_row_update_counter();

  LOG("Add task '%s' to task list '%s'", td->uid, td->list_uid);
}

static void on_task_list_search(GtkSearchEntry *entry, gpointer user_data) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  errands_task_list_filter_by_text(text);
}

void errands_task_list_build() {
  LOG("Creating task list");

  // Toolbar View
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "width-request", 360, NULL);

  // Header Bar
  GtkWidget *hb = adw_header_bar_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  g_object_set(hb, "show-title", false, NULL);

  // Search Bar
  GtkWidget *sb = gtk_search_bar_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), sb);
  GtkWidget *s_btn = gtk_toggle_button_new();
  g_object_set(s_btn, "icon-name", "errands-search-symbolic", NULL);
  gtk_widget_add_css_class(s_btn, "flat");
  adw_header_bar_pack_end(ADW_HEADER_BAR(hb), s_btn);
  g_object_bind_property(s_btn, "active", sb, "search-mode-enabled",
                         G_BINDING_BIDIRECTIONAL);
  GtkWidget *se = gtk_search_entry_new();
  g_signal_connect(se, "search-changed", G_CALLBACK(on_task_list_search), NULL);
  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(sb), GTK_EDITABLE(se));
  gtk_search_bar_set_key_capture_widget(GTK_SEARCH_BAR(sb), tb);
  gtk_search_bar_set_child(GTK_SEARCH_BAR(sb), se);

  // Entry
  GtkWidget *task_entry = adw_entry_row_new();
  g_object_set(task_entry, "title", "Add Task", "activatable", false,
               "margin-start", 12, "margin-end", 12, NULL);
  gtk_widget_add_css_class(task_entry, "card");
  g_signal_connect(task_entry, "entry-activated", G_CALLBACK(on_task_added),
                   NULL);
  GtkWidget *entry_clamp = adw_clamp_new();
  g_object_set(entry_clamp, "child", task_entry, "tightening-threshold", 300,
               "maximum-size", 1000, "margin-top", 6, "margin-bottom", 6, NULL);
  state.task_list_entry = gtk_revealer_new();
  g_object_set(state.task_list_entry, "child", entry_clamp, "transition-type",
               GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP, "margin-start", 12,
               "margin-end", 12, NULL);
  g_object_bind_property(s_btn, "active", state.task_list_entry, "reveal-child",
                         G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  // Tasks Box
  state.task_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *data = state.t_data->pdata[i];
    if (!strcmp(data->parent, "") && !data->deleted)
      gtk_box_append(GTK_BOX(state.task_list),
                     GTK_WIDGET(errands_task_new(data)));
  }

  GtkWidget *tbox_clamp = adw_clamp_new();
  g_object_set(tbox_clamp, "tightening-threshold", 300, "maximum-size", 1000,
               "margin-start", 12, "margin-end", 12, NULL);
  adw_clamp_set_child(ADW_CLAMP(tbox_clamp), state.task_list);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  g_object_set(scrl, "child", tbox_clamp, "propagate-natural-height", true,
               "propagate-natural-width", true, NULL);

  // VBox
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(vbox), state.task_list_entry);
  gtk_box_append(GTK_BOX(vbox), scrl);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), vbox);

  // Create task list page in the view stack
  adw_view_stack_add_named(ADW_VIEW_STACK(state.stack), tb,
                           "errands_task_list_page");

  // Sort tasks
  // errands_task_list_sort_by_completion(state.task_list);
  state.current_uid = "";
}

void errands_task_list_filter_by_uid(const char *uid) {
  GPtrArray *tasks = get_children(state.task_list);

  if (!strcmp(uid, "")) {
    for (int i = 0; i < tasks->len; i++) {
      ErrandsTask *task = tasks->pdata[i];
      if (!task->data->deleted)
        gtk_widget_set_visible(GTK_WIDGET(task), true);
    }
    return;
  }

  for (int i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    if (!strcmp(uid, task->data->list_uid) && !task->data->deleted)
      gtk_widget_set_visible(GTK_WIDGET(task), true);
    else
      gtk_widget_set_visible(GTK_WIDGET(task),
                             !strcmp(task->data->list_uid, uid) &&
                                 !task->data->deleted);
  }
}

void errands_task_list_filter_by_text(const char *text) {
  GPtrArray *tasks = get_children(state.task_list);
  // Search all tasks
  if (!strcmp(state.current_uid, "")) {
    for (int i = 0; i < tasks->len; i++) {
      ErrandsTask *task = tasks->pdata[i];
      bool contains = string_contains(task->data->text, text) ||
                      string_contains(task->data->notes, text) ||
                      string_array_contains(task->data->tags, text);
      gtk_widget_set_visible(GTK_WIDGET(task),
                             !task->data->deleted && contains);
    }
    return;
  }
  // Search for task list uid
  for (int i = 0; i < tasks->len; i++) {
    ErrandsTask *task = tasks->pdata[i];
    bool contains = string_contains(task->data->text, text) ||
                    string_contains(task->data->notes, text) ||
                    string_array_contains(task->data->tags, text);
    gtk_widget_set_visible(GTK_WIDGET(task), !task->data->deleted &&
                                                 !strcmp(task->data->list_uid,
                                                         state.current_uid) &&
                                                 contains);
  }
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
    task = last_cmp_task =
        (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));

  // Sort the rest of the tasks
  while (task) {
    if (task->data->completed) {
      gtk_box_reorder_child_after(GTK_BOX(task_list), GTK_WIDGET(task),
                                  GTK_WIDGET(last_cmp_task));
      last_cmp_task =
          (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
    }
    task = (ErrandsTask *)gtk_widget_get_prev_sibling(GTK_WIDGET(task));
  }
}
