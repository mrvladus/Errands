#include "task-list.h"
#include "data.h"
#include "gtk/gtkshortcut.h"
#include "settings.h"
#include "sidebar.h"
#include "sync.h"
#include "task-menu.h"
#include "task.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static size_t tasks_stack_size = 0, current_start = 0;
static GPtrArray *current_task_list = NULL;
static const char *search_query = NULL;
static ErrandsTask *measuring_task = NULL;

static ErrandsTask *entry_task = NULL;
static TaskData *entry_task_data = NULL;

static int __get_tasks_stack_size();

static void on_task_list_entry_activated_cb(ErrandsTaskList *self);
static void on_task_list_entry_text_changed_cb(ErrandsTaskList *self);
static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry);
static void on_adjustment_value_changed_cb(GtkAdjustment *adj, ErrandsTaskList *self);
static void on_motion_cb(GtkEventControllerMotion *ctrl, gdouble x, gdouble y, ErrandsTaskList *self);

static void on_focus_entry_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_entry_task_menu_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)

static void errands_task_list_dispose(GObject *gobject) {
  if (entry_task) g_object_run_dispose(G_OBJECT(entry_task));
  errands_task_data_free(entry_task_data);
  if (measuring_task) g_object_run_dispose(G_OBJECT(measuring_task));
  if (current_task_list) g_ptr_array_free(current_task_list, true);
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST);
  G_OBJECT_CLASS(errands_task_list_parent_class)->dispose(gobject);
}

static void errands_task_list_class_init(ErrandsTaskListClass *class) {
  g_type_ensure(ERRANDS_TYPE_TASK_MENU);

  G_OBJECT_CLASS(class)->dispose = errands_task_list_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/io/github/mrvladus/Errands/ui/task-list.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, title);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, search_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, search_bar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, search_entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry_apply_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry_menu_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, adj);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, task_menu);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, motion_ctrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, scrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, top_spacer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, task_list);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_sort_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_text_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_search_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_adjustment_value_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_motion_cb);
}

static void errands_task_list_init(ErrandsTaskList *self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  GSimpleActionGroup *ag = errands_add_action_group(self, "task-list");
  errands_add_action(ag, "focus-entry", on_focus_entry_action_cb, self, NULL);
  errands_add_action(ag, "show-entry-task-menu", on_entry_task_menu_action_cb, self, NULL);

  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(self->search_bar), GTK_EDITABLE(self->search_entry));
  measuring_task = errands_task_new();
  // Create entry task
  entry_task = errands_task_new();
  entry_task_data = errands_task_data_create_task(NULL, NULL, "");
  errands_task_set_data(entry_task, entry_task_data);
  // Create Tasks widgets
  tasks_stack_size = __get_tasks_stack_size();
  for_range(i, 0, tasks_stack_size) {
    ErrandsTask *task = errands_task_new();
    gtk_box_append(GTK_BOX(self->task_list), GTK_WIDGET(task));
    gtk_widget_set_visible(GTK_WIDGET(task), false);
  }
  LOG("Task List: Created %zu Tasks widgets in the stack", tasks_stack_size);
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

// ---------- PRIVATE FUNCTIONS ---------- //

static bool __task_has_any_collapsed_parent(TaskData *data) {
  for (TaskData *task = data->parent; task; task = task->parent)
    if (!errands_data_get_expanded(task->ical)) return true;
  return false;
}

static bool __task_has_any_pinned_parent(TaskData *data) {
  for (TaskData *task = data->parent; task; task = task->parent)
    if (errands_data_get_pinned(task->ical)) return true;
  return false;
}

static bool __task_has_any_due_parent(TaskData *data) {
  for (TaskData *task = data->parent; task; task = task->parent)
    if (errands_data_is_due(task->ical)) return true;
  return false;
}

static bool __task_match_search_query(TaskData *data) {
  const char *text = errands_data_get_text(data->ical);
  const char *notes = errands_data_get_notes(data->ical);
  if (text && g_strstr_len(text, -1, search_query)) return true;
  if (notes && g_strstr_len(notes, -1, search_query)) return true;
  g_auto(GStrv) tags = errands_data_get_tags(data->ical);
  if (tags) for_range(i, 0, g_strv_length(tags)) {
      if (g_strstr_len(tags[i], -1, search_query)) return true;
    }
  return false;
}

static bool __task_has_any_search_matched_parent(TaskData *data) {
  for (TaskData *task = data->parent; task; task = task->parent)
    if (__task_match_search_query(task)) return true;
  return false;
}

static int __calculate_height(ErrandsTaskList *self) {
  static int cached_height = 0;
  static GPtrArray *last_task_list = NULL;
  static ErrandsTaskListPage last_page = ERRANDS_TASK_LIST_PAGE_ALL;
  static const char *last_search_query = NULL;
  // Check if we can use cached value
  if (last_task_list == current_task_list && last_page == self->page && last_search_query == search_query)
    return cached_height;
  // Calculate fresh height
  int height = 0;
  GtkRequisition min_size, nat_size;
  bool show_completed = errands_settings_get(SETTING_SHOW_COMPLETED).b;
  bool show_cancelled = errands_settings_get(SETTING_SHOW_CANCELLED).b;
  for_range(i, 0, current_task_list->len) {
    TaskData *data = g_ptr_array_index(current_task_list, i);
    CONTINUE_IF(errands_data_get_deleted(data->ical) || errands_data_get_deleted(data->list->ical));
    CONTINUE_IF(errands_data_is_completed(data->ical) && !show_completed);
    CONTINUE_IF(errands_data_get_cancelled(data->ical) && !show_cancelled);
    CONTINUE_IF(__task_has_any_collapsed_parent(data));
    CONTINUE_IF(self->page == ERRANDS_TASK_LIST_PAGE_PINNED && !__task_has_any_pinned_parent(data));
    CONTINUE_IF(self->page == ERRANDS_TASK_LIST_PAGE_TODAY && !__task_has_any_due_parent(data));
    if (search_query && !STR_EQUAL(search_query, "")) {
      CONTINUE_IF(!__task_match_search_query(data) && !__task_has_any_search_matched_parent(data));
    }
    errands_task_set_data(measuring_task, data);
    gtk_widget_get_preferred_size(GTK_WIDGET(measuring_task), &min_size, &nat_size);
    height += nat_size.height;
  }
  // Cache the result
  cached_height = height;
  last_task_list = current_task_list;
  last_page = self->page;
  last_search_query = search_query;

  return height;
}

static int __get_tasks_stack_size() {
  static int cached_size = 0; // Cache the result
  if (cached_size != 0) return cached_size;
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
  int max_tasks = max_height / (46 + 5);   // Get number of maximum tasks on the screen
  cached_size = max_tasks + max_tasks / 2; // Add half of that as margin and set stack size

  return cached_size;
}

// ---------- TASKS RECYCLER ---------- //

void errands_task_list_redraw_tasks(ErrandsTaskList *self) {
  static const uint8_t indent_px = 15;
  if (current_task_list->len == 0) return;
  bool show_completed = errands_settings_get(SETTING_SHOW_COMPLETED).b;
  bool show_cancelled = errands_settings_get(SETTING_SHOW_CANCELLED).b;
  g_autoptr(GPtrArray) children = get_children(self->task_list);
  size_t indent_offset = 0;
  for (size_t i = 0, j = current_start; i < MIN(tasks_stack_size, current_task_list->len - current_start); ++i, ++j) {
    ErrandsTask *task = g_ptr_array_index(children, i);
    TaskData *data = g_ptr_array_index(current_task_list, j);
    CONTINUE_IF(errands_data_get_deleted(data->ical) || errands_data_get_deleted(data->list->ical));
    CONTINUE_IF(errands_data_is_completed(data->ical) && !show_completed);
    CONTINUE_IF(errands_data_get_cancelled(data->ical) && !show_cancelled);
    size_t indent = errands_task_data_get_indent_level(data);
    bool show = true;
    bool match_search = true;
    bool match_page = true;
    if (search_query && !STR_EQUAL(search_query, "")) {
      if (!__task_has_any_search_matched_parent(data)) {
        match_search = __task_match_search_query(data);
        if (match_search) indent_offset = -indent;
      }
    }
    if (self->page == ERRANDS_TASK_LIST_PAGE_TODAY) {
      if (!__task_has_any_due_parent(data)) {
        match_page = errands_data_is_due(data->ical);
        if (match_page) indent_offset = -indent;
      }
    } else if (self->page == ERRANDS_TASK_LIST_PAGE_PINNED) {
      if (!__task_has_any_pinned_parent(data)) {
        match_page = errands_data_get_pinned(data->ical);
        if (match_page) indent_offset = -indent;
      }
    }
    show = match_search && match_page;
    CONTINUE_IF(!show || __task_has_any_collapsed_parent(data));
    gtk_widget_set_margin_start(GTK_WIDGET(task), (indent + indent_offset) * indent_px);
    errands_task_set_data(task, data);
  }
}

static void on_adjustment_value_changed_cb(GtkAdjustment *adj, ErrandsTaskList *self) {
  // Get current scroll position
  static double old_scroll_position = 0.0f;
  const double new_scroll_position = gtk_adjustment_get_value(adj);
  const double delta = new_scroll_position - old_scroll_position;
  const bool scrolling_down = delta > 0;

  // Handle large jumps (scrollbar clicks)
  const double page_size = gtk_adjustment_get_page_size(adj);
  const double total_height = gtk_adjustment_get_upper(adj);
  const bool is_large_jump = fabs(delta) > page_size * 1.5;

  if (is_large_jump) {
    LOG("Large jump detected: delta=%f, page_size=%f", delta, page_size);

    // Calculate which task should be at the top based on scroll position
    int total_visible_height = __calculate_height(self);
    if (total_visible_height <= 0) return;

    // Estimate which task index should be at the top
    double scroll_ratio = new_scroll_position / (total_height - page_size);
    scroll_ratio = CLAMP(scroll_ratio, 0.0, 1.0);

    // Count visible tasks (tasks that match filters)
    int visible_count = 0;
    bool show_completed = errands_settings_get(SETTING_SHOW_COMPLETED).b;
    bool show_cancelled = errands_settings_get(SETTING_SHOW_CANCELLED).b;
    for (size_t i = 0; i < current_task_list->len; i++) {
      TaskData *data = g_ptr_array_index(current_task_list, i);
      if (errands_data_get_deleted(data->ical) || errands_data_get_deleted(data->list->ical)) continue;
      if (errands_data_is_completed(data->ical) && !show_completed) continue;
      if (errands_data_get_cancelled(data->ical) && !show_cancelled) continue;
      if (__task_has_any_collapsed_parent(data)) continue;
      if (self->page == ERRANDS_TASK_LIST_PAGE_PINNED && !__task_has_any_pinned_parent(data)) continue;
      if (self->page == ERRANDS_TASK_LIST_PAGE_TODAY && !__task_has_any_due_parent(data)) continue;
      if (search_query && !STR_EQUAL(search_query, ""))
        if (!__task_match_search_query(data) && !__task_has_any_search_matched_parent(data)) continue;
      visible_count++;
    }

    if (visible_count > 0) {
      // Calculate new start index
      size_t new_start = (size_t)(scroll_ratio * (visible_count - 1));
      new_start = MIN(new_start, current_task_list->len - tasks_stack_size);
      new_start = MAX(new_start, 0);

      LOG("Large jump: scroll_ratio=%f, new_start=%zu, visible_count=%d", scroll_ratio, new_start, visible_count);

      // Update current_start
      current_start = new_start;

      // Reset all widgets
      g_autoptr(GPtrArray) children = get_children(self->task_list);
      for (size_t i = 0; i < children->len; i++) { gtk_widget_set_visible(g_ptr_array_index(children, i), false); }

      // Update spacer height based on new start position
      int spacer_height = 0;
      int counted = 0;

      for (size_t i = 0; i < current_start && counted < current_start; i++) {
        TaskData *data = g_ptr_array_index(current_task_list, i);
        if (errands_data_get_deleted(data->ical) || errands_data_get_deleted(data->list->ical)) continue;
        if (errands_data_is_completed(data->ical) && !show_completed) continue;
        if (errands_data_get_cancelled(data->ical) && !show_cancelled) continue;
        if (__task_has_any_collapsed_parent(data)) continue;
        if (self->page == ERRANDS_TASK_LIST_PAGE_PINNED && !__task_has_any_pinned_parent(data)) continue;
        if (self->page == ERRANDS_TASK_LIST_PAGE_TODAY && !__task_has_any_due_parent(data)) continue;
        if (search_query && !STR_EQUAL(search_query, "")) {
          if (!__task_match_search_query(data) && !__task_has_any_search_matched_parent(data)) continue;
        }

        errands_task_set_data(measuring_task, data);
        GtkRequisition min_size, nat_size;
        gtk_widget_get_preferred_size(GTK_WIDGET(measuring_task), &min_size, &nat_size);
        spacer_height += nat_size.height;
        counted++;
      }

      gtk_widget_set_size_request(self->top_spacer, -1, spacer_height);
      gtk_widget_set_size_request(self->task_list, -1, total_visible_height - spacer_height);

      // Redraw tasks
      errands_task_list_redraw_tasks(self);
    }
  } else {
    // Normal incremental scrolling
    g_autoptr(GPtrArray) children = get_children(self->task_list);
    if (!children || children->len < 3) return;

    const double viewport_height = page_size;
    const double recycle_threshold_down = -viewport_height * 0.5f;
    const double recycle_threshold_up = viewport_height * 1.5f;

    const int current_spacer_height = gtk_widget_get_height(self->top_spacer);
    const int total_list_height = __calculate_height(self);

    if (scrolling_down) {
      // Scrolling down: recycle widgets that have scrolled above the viewport
      bool recycled = false;
      int moved_height = 0;

      for (size_t i = 0; i < children->len; ++i) {
        GtkWidget *child = g_ptr_array_index(children, i);
        if (!gtk_widget_get_visible(child)) continue;

        graphene_rect_t bounds = {0};
        if (!gtk_widget_compute_bounds(child, self->scrl, &bounds)) continue;

        // If widget is above the viewport and we have more tasks to show
        if (bounds.origin.y + bounds.size.height <= recycle_threshold_down &&
            current_start + children->len < current_task_list->len) {
          moved_height += (int)bounds.size.height;

          // Move widget to the end of the list
          GtkWidget *last_widget = children->pdata[children->len - 1];
          gtk_box_reorder_child_after(GTK_BOX(self->task_list), child, last_widget);
          gtk_widget_set_visible(child, false);

          recycled = true;
          current_start++;
        } else {
          break;
        }
      }

      if (recycled) {
        // Update spacer height
        gtk_widget_set_size_request(self->top_spacer, -1, MAX(0, current_spacer_height + moved_height));

        // Update task list height
        int visible_height = total_list_height - (current_spacer_height + moved_height);
        gtk_widget_set_size_request(self->task_list, -1, MAX(0, visible_height));

        errands_task_list_redraw_tasks(self);
      }
    } else {
      // Scrolling up: recycle widgets that have scrolled below the viewport
      bool recycled = false;
      int moved_height = 0;

      for (int i = children->len - 1; i >= 0; --i) {
        GtkWidget *child = children->pdata[i];
        if (!gtk_widget_get_visible(child)) continue;

        graphene_rect_t bounds = {0};
        if (!gtk_widget_compute_bounds(child, self->scrl, &bounds)) continue;

        // If widget is below the viewport and we can scroll up
        if (current_start > 0 && bounds.origin.y >= recycle_threshold_up) {
          moved_height += (int)bounds.size.height;

          // Move widget to the beginning of the list
          gtk_box_reorder_child_after(GTK_BOX(self->task_list), child, NULL);
          gtk_widget_set_visible(child, false);

          recycled = true;
          current_start--;
        } else {
          break;
        }
      }

      if (recycled) {
        // Update spacer height
        gtk_widget_set_size_request(self->top_spacer, -1, MAX(0, current_spacer_height - moved_height));

        // Update task list height
        int visible_height = total_list_height - MAX(0, current_spacer_height - moved_height);
        gtk_widget_set_size_request(self->task_list, -1, MAX(0, visible_height));

        errands_task_list_redraw_tasks(self);
      }
    }
  }

  old_scroll_position = new_scroll_position;

  // Reset spacer when at the top
  if (new_scroll_position <= 1.0) { // Use small epsilon instead of exact 0
    gtk_widget_set_size_request(self->top_spacer, -1, 0);
    int total_height = __calculate_height(self);
    gtk_widget_set_size_request(self->task_list, -1, total_height);
    current_start = 0;
    errands_task_list_redraw_tasks(self);
  }
}

static void on_motion_cb(GtkEventControllerMotion *ctrl, gdouble x, gdouble y, ErrandsTaskList *self) {
  self->x = x;
  self->y = y;
}

// ---------- ACTIONS ---------- //

static void on_focus_entry_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  gtk_widget_grab_focus(self->entry);
}

static void on_entry_task_menu_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  graphene_rect_t rect = {0};
  bool res = gtk_widget_compute_bounds(GTK_WIDGET(self->entry_menu_btn), GTK_WIDGET(self), &rect);
  UNUSED(res);
  errands_task_menu_show(entry_task, rect.origin.x, rect.origin.y - rect.size.height, ERRANDS_TASK_MENU_MODE_ENTRY);
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_update_title(ErrandsTaskList *self) {
  switch (self->page) {
  case ERRANDS_TASK_LIST_PAGE_ALL: adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("All Tasks")); break;
  case ERRANDS_TASK_LIST_PAGE_TODAY: {
    // TODO: use current_task_list?
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("Tasks for Today"));
    size_t total = 0, completed = 0;
    for_range(i, 0, errands_data_lists->len) {
      ListData *list = g_ptr_array_index(errands_data_lists, i);
      CONTINUE_IF(errands_data_get_deleted(list->ical));
      g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(list);
      for_range(j, 0, tasks->len) {
        icalcomponent *ical = g_ptr_array_index(tasks, j);
        if (!errands_data_get_deleted(ical) && errands_data_is_due(ical)) {
          if (errands_data_is_completed(ical)) completed++;
          total++;
        }
      }
    }
    const char *stats = tmp_str_printf("%s %zu / %zu", _("Completed:"), completed, total);
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(self->title), total > 0 ? stats : "");
    gtk_widget_set_visible(self->scrl, total > 0);
    return;
  } break;
  case ERRANDS_TASK_LIST_PAGE_PINNED: {
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("Pinned Tasks"));
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(self->title), "");
    size_t pinned = 0;
    for_range(i, 0, current_task_list->len) {
      TaskData *data = g_ptr_array_index(current_task_list, i);
      CONTINUE_IF(errands_data_get_deleted(data->ical));
      if (errands_data_get_pinned(data->ical)) pinned++;
    }
    gtk_widget_set_visible(self->scrl, pinned > 0);
    return;
  } break;
  case ERRANDS_TASK_LIST_PAGE_TASK_LIST:
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), errands_data_get_list_name(self->data->ical));
    break;
  }
  // Retrieve tasks and count completed and total tasks
  size_t total = 0, completed = 0;
  for_range(i, 0, current_task_list->len) {
    TaskData *data = g_ptr_array_index(current_task_list, i);
    CONTINUE_IF(errands_data_get_deleted(data->ical) || errands_data_get_deleted(data->list->ical));
    if (!icaltime_is_null_date(errands_data_get_completed(data->ical))) completed++;
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
  gtk_widget_set_visible(self->entry_box, false);
  errands_task_list_reload(self, false);
}

void errands_task_list_show_all_tasks(ErrandsTaskList *self) {
  LOG("Task List: Show all tasks");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_ALL;
  gtk_widget_set_visible(self->entry_box, false);
  errands_task_list_reload(self, false);
}

void errands_task_list_show_task_list(ErrandsTaskList *self, ListData *data) {
  self->data = data;
  self->page = ERRANDS_TASK_LIST_PAGE_TASK_LIST;
  gtk_widget_set_visible(self->entry_box, true);
  errands_task_list_reload(self, false);
}

void errands_task_list_show_pinned(ErrandsTaskList *self) {
  LOG("Task List: Show pinned");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_PINNED;
  gtk_widget_set_visible(self->entry_box, false);
  errands_task_list_reload(self, false);
}

static void __reset_scroll_cb(ErrandsTaskList *self) { gtk_adjustment_set_value(self->adj, 0.0); }

void errands_task_list_reload(ErrandsTaskList *self, bool save_scroll_pos) {
  // FIXME: correct scroll position
  if (!save_scroll_pos) current_start = 0;
  if (current_task_list) g_ptr_array_set_size(current_task_list, 0);
  else current_task_list = g_ptr_array_new();
  if (self->data) errands_list_data_get_flat_list(self->data, current_task_list);
  else errands_data_get_flat_list(current_task_list);
  gtk_widget_set_size_request(self->top_spacer, -1, 0);
  gtk_widget_set_size_request(self->task_list, -1, __calculate_height(self));
  errands_task_list_update_title(self);
  if (!save_scroll_pos) g_idle_add_once((GSourceOnceFunc)__reset_scroll_cb, self);
  for_child_in_parent(child, self->task_list) gtk_widget_set_visible(child, false);
  errands_task_list_redraw_tasks(self);
}

// ---------- CALLBACKS ---------- //

static void on_task_list_entry_activated_cb(ErrandsTaskList *self) {
  if (!self->data) return;
  // Get text
  const char *text = gtk_editable_get_text(GTK_EDITABLE(self->entry));
  g_autofree gchar *dup = g_strdup(text);
  char *stripped = g_strstrip(dup);

  const char *list_uid = self->data->uid;
  if (STR_EQUAL(stripped, "") || STR_EQUAL(list_uid, "")) return;

  // Create new top-level task from entry task
  TaskData *data = errands_task_data_new(entry_task_data->ical, NULL, self->data);
  errands_data_set_uid(data->ical, generate_uuid4());
  errands_data_set_text(data->ical, stripped);
  errands_data_set_created(data->ical, icaltime_get_date_time_now());
  icalcomponent_add_component(self->data->ical, data->ical);
  errands_list_data_save(self->data);
  // Reload entry task
  entry_task->data->ical = icalcomponent_new(ICAL_VTODO_COMPONENT);
  // Reset text
  g_object_set(self->entry, "text", "", NULL);
  // Update UI
  errands_sidebar_task_list_row_update(errands_sidebar_task_list_row_get(data->list));
  errands_sidebar_update_filter_rows();
  LOG("Add task '%s' to task list '%s'", errands_data_get_uid(data->ical), list_uid);
  errands_list_data_sort(self->data);
  errands_task_list_reload(self, false);
  errands_sync_create_task(data);
}

static void on_task_list_entry_text_changed_cb(ErrandsTaskList *self) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(self->entry));
  gtk_widget_set_sensitive(self->entry_apply_btn, text && !STR_EQUAL(text, ""));
}

static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry) {
  search_query = gtk_editable_get_text(GTK_EDITABLE(entry));
  LOG("Search query changed to '%s'", search_query);
  errands_task_list_reload(self, false);
}
