#include "task-list.h"
#include "data.h"
#include "glib.h"
#include "sidebar.h"
#include "state.h"
#include "task.h"
#include "utils.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libical/ical.h>
#include <stdbool.h>
#include <stddef.h>

static size_t tasks_stack_size = 0, current_start = 0;
static GPtrArray *current_task_list = NULL;
static const char *search_query = NULL;
static ErrandsTask *measuring_task = NULL;
static double scroll_position = 0.0f;

static void on_task_list_entry_activated_cb(AdwEntryRow *entry, ErrandsTaskList *self);
static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry);
static void on_adjustment_value_changed_cb(GtkAdjustment *adj, ErrandsTaskList *self);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)

static void errands_task_list_dispose(GObject *gobject) {
  g_object_run_dispose(G_OBJECT(measuring_task));
  if (current_task_list) g_ptr_array_free(current_task_list, true);
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
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry_clamp);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, adj);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, scrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, top_spacer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, task_list);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_sort_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_search_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_adjustment_value_changed_cb);
}

static void errands_task_list_init(ErrandsTaskList *self) {
  LOG("Task List: Create");
  gtk_widget_init_template(GTK_WIDGET(self));
  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(self->search_bar), GTK_EDITABLE(self->search_entry));
  measuring_task = errands_task_new();
  // Get maximum monitor height
  GdkDisplay *display = gdk_display_get_default();
  GListModel *monitors = gdk_display_get_monitors(display);
  int max_height = 0;
  GdkRectangle rect = {0};
  for_range(i, 0, g_list_model_get_n_items(monitors)) {
    GdkMonitor *monitor = g_list_model_get_item(monitors, i);
    gdk_monitor_get_geometry(monitor, &rect);
    if (rect.height > max_height) max_height = rect.height;
  }
  int max_tasks = max_height / (46 + 8);        // Get number of maximum tasks on the screen
  tasks_stack_size = max_tasks + max_tasks / 2; // Add half of that as margin and set stack size
  // Create Tasks widgets
  for_range(i, 0, tasks_stack_size) {
    ErrandsTask *task = errands_task_new();
    gtk_box_append(GTK_BOX(self->task_list), GTK_WIDGET(task));
    gtk_widget_set_visible(GTK_WIDGET(task), false);
  }
  LOG("Task List: Created %zu Tasks", tasks_stack_size);
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

// ---------- PRIVATE FUNCTIONS ---------- //

static bool errands_task_list__task_has_any_collapsed_parent(TaskData *data) {
  bool out = false;
  TaskData *task = data->parent;
  while (task) {
    if (!errands_data_get_bool(task->data, DATA_PROP_EXPANDED)) {
      out = true;
      break;
    }
    task = task->parent;
  }
  return out;
}

static bool errands_task_list__task_has_any_due_parent(TaskData *data) {
  bool out = false;
  TaskData *task = data->parent;
  icaltimetype today = icaltime_today();
  while (task) {
    icaltimetype due = errands_data_get_time(task->data, DATA_PROP_DUE_TIME);
    if (icaltime_compare_date_only(due, today) < 1) {
      out = true;
      break;
    }
    task = task->parent;
  }
  return out;
}

static bool errands_task_list__task_match_search_query(TaskData *data) {
  if (STR_CONTAINS_CASE(errands_data_get_str(data->data, DATA_PROP_TEXT), search_query)) return true;
  if (STR_CONTAINS_CASE(errands_data_get_str(data->data, DATA_PROP_NOTES), search_query)) return true;
  g_auto(GStrv) tags = errands_data_get_strv(data->data, DATA_PROP_TAGS);
  if (g_strv_contains((const gchar *const *)tags, search_query)) return true;
  return false;
}

static bool errands_task_list__task_match_search_or_has_matched_child(TaskData *data) {
  if (errands_task_list__task_match_search_query(data)) return true;
  g_autoptr(GPtrArray) sub_tasks = g_ptr_array_new();
  errands_task_data_get_flat_list(data, sub_tasks);
  for_range(i, 0, sub_tasks->len) {
    TaskData *sub_task = g_ptr_array_index(sub_tasks, i);
    if (errands_task_list__task_match_search_query(sub_task)) return true;
  }
  return false;
}

static int errands_task_list__calculate_height(ErrandsTaskList *self) {
  // LOG_NO_LN("Task List: Calculating height ... ");
  // TIMER_START;
  int height = 0;
  GtkRequisition min_size, nat_size;
  for_range(i, 0, current_task_list->len) {
    TaskData *data = g_ptr_array_index(current_task_list, i);
    CONTINUE_IF(errands_task_list__task_has_any_collapsed_parent(data));
    errands_task_set_data(measuring_task, data);
    gtk_widget_get_preferred_size(GTK_WIDGET(measuring_task), &min_size, &nat_size);
    height += nat_size.height;
  }
  // LOG_NO_PREFIX("%d (%f sec.)", height, TIMER_ELAPSED_MS);
  return height;
}

static void errands_task_list__reset_scroll_cb(ErrandsTaskList *self) { gtk_adjustment_set_value(self->adj, 0.0); }

// ---------- TASKS RECYCLER ---------- //

void errands_task_list_redraw_tasks(ErrandsTaskList *self) {
  // LOG("Task List: Redraw Tasks");
  static uint8_t indent_px = 15;
  if (current_task_list->len == 0) return;
  icaltimetype today = icaltime_today();
  g_autoptr(GPtrArray) children = get_children(self->task_list);
  for (size_t i = 0, j = current_start; i < MIN(tasks_stack_size, current_task_list->len - current_start);) {
    ErrandsTask *task = g_ptr_array_index(children, i++);
    TaskData *data = g_ptr_array_index(current_task_list, j++);
    // Don't show sub-tasks of collapsed parents
    CONTINUE_IF(errands_task_list__task_has_any_collapsed_parent(data));
    // Show only today tasks for today page
    if (self->page == ERRANDS_TASK_LIST_PAGE_TODAY) {
      // Check if any parent is due - then show task anyway, else check due date of the task
      if (!errands_task_list__task_has_any_due_parent(data)) {
        // errands_task_data_is_due(data);
        icaltimetype due = errands_data_get_time(data->data, DATA_PROP_DUE_TIME);
        CONTINUE_IF(icaltime_is_null_date(due));
        CONTINUE_IF(icaltime_compare_date_only(due, today) == 1);
      }
    }
    // Show only today tasks for trash page
    // else if (self->page == ERRANDS_TASK_LIST_PAGE_TRASH) {
    //   if (!errands_task_list__task_has_any_trash_parent(data))
    //     CONTINUE_IF(!errands_data_get_bool(data->data, DATA_PROP_TRASH));
    // }
    // Search query
    if (search_query && !STR_EQUAL(search_query, ""))
      CONTINUE_IF(!errands_task_list__task_match_search_or_has_matched_child(data));
    errands_task_set_data(task, data);
    gtk_widget_set_margin_start(GTK_WIDGET(task), errands_task_data_get_indent_level(data) * indent_px);
  }
}

static void on_adjustment_value_changed_cb(GtkAdjustment *adj, ErrandsTaskList *self) {
  // TODO("Recycle by 5 tasks");
  double old_scroll_position = scroll_position;
  scroll_position = gtk_adjustment_get_value(adj);
  bool scrolling_down = scroll_position > old_scroll_position;
  g_autoptr(GPtrArray) children = get_children(self->task_list);
  if (!children || children->len < 3) return;
  GtkWidget *first_widget = children->pdata[0];
  GtkWidget *last_widget = children->pdata[children->len - 1];
  graphene_rect_t first_bounds, last_bounds;
  if (!gtk_widget_compute_bounds(first_widget, self->scrl, &first_bounds)) return;
  if (!gtk_widget_compute_bounds(last_widget, self->scrl, &last_bounds)) return;
  const float page_size = gtk_adjustment_get_page_size(adj);
  const float recycle_threshold_down = -page_size * 0.5f;
  const float recycle_threshold_up = page_size * 1.5f;
  int spacer_height = gtk_widget_get_height(self->top_spacer);
  if (scrolling_down) {
    int recycled = 0;
    int moved_height = 0;
    for (size_t i = 0; i < children->len; ++i) {
      GtkWidget *child = children->pdata[i];
      graphene_rect_t bounds;
      if (!gtk_widget_compute_bounds(child, self->scrl, &bounds)) continue;
      if (bounds.origin.y + bounds.size.height <= recycle_threshold_down &&
          current_start + children->len < current_task_list->len) {
        moved_height += gtk_widget_get_height(child) + 8;
        gtk_box_reorder_child_after(GTK_BOX(self->task_list), child, last_widget);
        gtk_widget_set_visible(child, false);
        recycled++;
        current_start++;
      } else break;
    }
    if (recycled > 0) {
      gtk_widget_set_size_request(self->top_spacer, -1, spacer_height + moved_height);
      gtk_widget_set_size_request(self->task_list, -1, gtk_widget_get_height(self->task_list) - moved_height);
      errands_task_list_redraw_tasks(self);
    }
  } else {
    int recycled = 0;
    int moved_height = 0;
    for (ssize_t i = children->len - 1; i >= 0; --i) {
      GtkWidget *child = children->pdata[i];
      graphene_rect_t bounds;
      if (!gtk_widget_compute_bounds(child, self->scrl, &bounds)) continue;
      if (bounds.origin.y >= recycle_threshold_up && current_start > 0) {
        moved_height += gtk_widget_get_height(child) + 8;
        gtk_box_reorder_child_after(GTK_BOX(self->task_list), child, NULL);
        gtk_widget_set_visible(child, false);
        recycled++;
        current_start--;
      } else break;
    }
    if (recycled > 0) {
      gtk_widget_set_size_request(self->top_spacer, -1, MAX(0, spacer_height - moved_height));
      gtk_widget_set_size_request(self->task_list, -1, gtk_widget_get_height(self->task_list) + moved_height);
      errands_task_list_redraw_tasks(self);
    }
  }
  if (current_start == 0) gtk_widget_set_size_request(self->top_spacer, -1, 0);
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_update_title(ErrandsTaskList *self) {
  switch (self->page) {
  case ERRANDS_TASK_LIST_PAGE_ALL: adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("All Tasks")); break;
  case ERRANDS_TASK_LIST_PAGE_TODAY: {
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("Today Tasks"));
    size_t total = 0, completed = 0;
    icaltimetype today = icaltime_today();
    for_range(i, 0, errands_data_lists->len) {
      ListData *list = g_ptr_array_index(errands_data_lists, i);
      g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(list);
      for_range(j, 0, tasks->len) {
        icalcomponent *data = g_ptr_array_index(tasks, j);
        bool deleted = errands_data_get_bool(data, DATA_PROP_DELETED);
        icaltimetype due_date = errands_data_get_time(data, DATA_PROP_DUE_TIME);
        if (!deleted && !icaltime_is_null_time(due_date) && icaltime_compare_date_only(due_date, today) < 1) {
          if (!icaltime_is_null_time(errands_data_get_time(data, DATA_PROP_COMPLETED_TIME))) completed++;
          total++;
        }
      }
    }
    const char *stats = tmp_str_printf("%s %zu / %zu", _("Completed:"), completed, total);
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(self->title), total > 0 ? stats : "");
    gtk_widget_set_visible(self->scrl, total > 0);
    return;
  } break;
  // case ERRANDS_TASK_LIST_PAGE_TRASH: {
  //   adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("Trash"));
  //   adw_window_title_set_subtitle(ADW_WINDOW_TITLE(self->title), "");
  //   size_t trashed = 0;
  //   for_range(i, 0, current_task_list->len) {
  //     TaskData *data = g_ptr_array_index(current_task_list, i);
  //     if (errands_data_get_bool(data->data, DATA_PROP_TRASH)) trashed++;
  //   }
  //   gtk_widget_set_visible(self->scrl, trashed > 0);
  //   gtk_widget_set_visible(self->clear_trash_btn, trashed > 0);
  //   return;
  // } break;
  case ERRANDS_TASK_LIST_PAGE_TASK_LIST:
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title),
                               errands_data_get_str(self->data->data, DATA_PROP_LIST_NAME));
    break;
  }
  // Retrieve tasks and count completed and total tasks
  size_t total = 0, completed = 0;
  for_range(i, 0, current_task_list->len) {
    TaskData *data = g_ptr_array_index(current_task_list, i);
    CONTINUE_IF(errands_data_get_bool(data->data, DATA_PROP_DELETED));
    if (!icaltime_is_null_date(errands_data_get_time(data->data, DATA_PROP_COMPLETED_TIME))) completed++;
    total++;
  }
  // Set subtitle with completed stats
  const char *stats = tmp_str_printf("%s %zu / %zu", _("Completed:"), completed, total);
  adw_window_title_set_subtitle(ADW_WINDOW_TITLE(self->title), total > 0 ? stats : "");
  gtk_widget_set_visible(self->scrl, total > 0);
}

void errands_task_list_show_today_tasks(ErrandsTaskList *self) {
  LOG("Task List: Show today tasks");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_TODAY;
  gtk_widget_set_visible(self->entry_clamp, false);
  errands_task_list_reload(self, false);
}

void errands_task_list_show_all_tasks(ErrandsTaskList *self) {
  LOG("Task List: Show all tasks");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_ALL;
  gtk_widget_set_visible(self->entry_clamp, false);
  errands_task_list_reload(self, false);
}

void errands_task_list_show_task_list(ErrandsTaskList *self, ListData *data) {
  self->data = data;
  self->page = ERRANDS_TASK_LIST_PAGE_TASK_LIST;
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->search_btn)))
    gtk_widget_set_visible(self->entry_clamp, true);
  errands_task_list_reload(self, false);
}

void errands_task_list_show_pinned(ErrandsTaskList *self) {
  LOG("Task List: Show pinned");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_PINNED;
  gtk_widget_set_visible(self->entry_clamp, false);
  errands_task_list_reload(self, false);
}

void errands_task_list_reload(ErrandsTaskList *self, bool save_scroll_pos) {
  // TODO: correct scroll position
  if (!save_scroll_pos) current_start = 0;
  if (current_task_list) g_ptr_array_set_size(current_task_list, 0);
  else current_task_list = g_ptr_array_new();
  if (self->data) errands_list_data_get_flat_list(self->data, current_task_list);
  else errands_data_get_flat_list(current_task_list);
  gtk_widget_set_size_request(self->top_spacer, -1, 0);
  gtk_widget_set_size_request(self->task_list, -1, errands_task_list__calculate_height(self));
  errands_task_list_update_title(self);
  if (!save_scroll_pos) g_idle_add_once((GSourceOnceFunc)errands_task_list__reset_scroll_cb, self);
  g_autoptr(GPtrArray) children = get_children(self->task_list);
  for_range(i, 0, children->len) gtk_widget_set_visible(g_ptr_array_index(children, i), false);
  errands_task_list_redraw_tasks(self);
}

// ---------- CALLBACKS ---------- //

static void on_task_list_entry_activated_cb(AdwEntryRow *entry, ErrandsTaskList *self) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  const char *list_uid = errands_data_get_str(self->data->data, DATA_PROP_LIST_UID);
  if (STR_EQUAL(text, "") || STR_EQUAL(list_uid, "")) return;
  TaskData *data = errands_task_data_create_task(self->data, NULL, text);
  g_ptr_array_add(self->data->children, data);
  errands_list_data_sort(self->data);
  errands_data_write_list(self->data);
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  errands_sidebar_task_list_row_update_counter(errands_sidebar_task_list_row_get(list_uid));
  errands_sidebar_all_row_update_counter(state.main_window->sidebar->all_row);
  LOG("Add task '%s' to task list '%s'", errands_data_get_str(data->data, DATA_PROP_UID),
      errands_data_get_str(data->data, DATA_PROP_LIST_UID));
  errands_task_list_reload(self, false);
}

static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry) {
  search_query = gtk_editable_get_text(GTK_EDITABLE(entry));
  LOG("Search query changed to '%s'", search_query);
}
