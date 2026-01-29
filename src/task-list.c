#include "task-list.h"
#include "data.h"
#include "gtk/gtk.h"
#include "settings.h"
#include "sidebar.h"
#include "sync.h"
#include "task.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <libical/ical.h>
#include <unistd.h>

static size_t tasks_stack_size = 0, current_start = 0;
static GPtrArray *current_task_list = NULL;
static const char *search_query = NULL;
static ErrandsTask *measuring_task = NULL;
static double scroll_position = 0.0f;

static void on_task_list_entry_activated_cb(ErrandsTaskList *self, AdwEntryRow *entry);
static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry);
// static void on_date_btn_clicked_cb(ErrandsTaskList *self, GtkButton *btn);
static void on_adjustment_value_changed_cb(GtkAdjustment *adj, ErrandsTaskList *self);
static void on_motion_cb(GtkEventControllerMotion *ctrl, gdouble x, gdouble y, ErrandsTaskList *self);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)

static void errands_task_list_dispose(GObject *gobject) {
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
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, adj);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, task_menu);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, motion_ctrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, scrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, top_spacer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, task_list);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_sort_dialog_show);
  // gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_date_btn_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_search_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_adjustment_value_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_motion_cb);
}

static void errands_task_list_init(ErrandsTaskList *self) {
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_DATE_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_TIME_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW);
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
  if (STR_CONTAINS_CASE(errands_data_get_text(data->ical), search_query)) return true;
  if (STR_CONTAINS_CASE(errands_data_get_notes(data->ical), search_query)) return true;
  g_auto(GStrv) tags = errands_data_get_tags(data->ical);
  if (tags && g_strv_contains((const gchar *const *)tags, search_query)) return true;
  return false;
}

static bool __task_has_any_search_matched_parent(TaskData *data) {
  for (TaskData *task = data->parent; task; task = task->parent)
    if (__task_match_search_query(task)) return true;
  return false;
}

static int __calculate_height(ErrandsTaskList *self) {
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
    errands_task_set_data(measuring_task, data);
    gtk_widget_get_preferred_size(GTK_WIDGET(measuring_task), &min_size, &nat_size);
    height += nat_size.height;
  }
  return height;
}

static void __reset_scroll_cb(ErrandsTaskList *self) { gtk_adjustment_set_value(self->adj, 0.0); }

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

static void on_motion_cb(GtkEventControllerMotion *ctrl, gdouble x, gdouble y, ErrandsTaskList *self) {
  self->x = x;
  self->y = y;
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_update_title(ErrandsTaskList *self) {
  switch (self->page) {
  case ERRANDS_TASK_LIST_PAGE_ALL: adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("All Tasks")); break;
  case ERRANDS_TASK_LIST_PAGE_TODAY: {
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
  gtk_widget_set_visible(self->entry, false);
  errands_task_list_reload(self, false);
}

void errands_task_list_show_all_tasks(ErrandsTaskList *self) {
  LOG("Task List: Show all tasks");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_ALL;
  gtk_widget_set_visible(self->entry, false);
  errands_task_list_reload(self, false);
}

void errands_task_list_show_task_list(ErrandsTaskList *self, ListData *data) {
  self->data = data;
  self->page = ERRANDS_TASK_LIST_PAGE_TASK_LIST;
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->search_btn))) gtk_widget_set_visible(self->entry, true);
  errands_task_list_reload(self, false);
}

void errands_task_list_show_pinned(ErrandsTaskList *self) {
  LOG("Task List: Show pinned");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_PINNED;
  gtk_widget_set_visible(self->entry, false);
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
  gtk_widget_set_size_request(self->task_list, -1, __calculate_height(self));
  errands_task_list_update_title(self);
  if (!save_scroll_pos) g_idle_add_once((GSourceOnceFunc)__reset_scroll_cb, self);
  g_autoptr(GPtrArray) children = get_children(self->task_list);
  for_range(i, 0, children->len) gtk_widget_set_visible(g_ptr_array_index(children, i), false);
  errands_task_list_redraw_tasks(self);
}

// ---------- CALLBACKS ---------- //

static void on_task_list_entry_activated_cb(ErrandsTaskList *self, AdwEntryRow *entry) {
  if (!self->data) return;
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  const char *list_uid = self->data->uid;
  if (STR_EQUAL(text, "") || STR_EQUAL(list_uid, "")) return;
  TaskData *data = errands_task_data_create_task(self->data, NULL, text);
  errands_list_data_sort(self->data);
  errands_list_data_save(self->data);
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  errands_sidebar_task_list_row_update(errands_sidebar_task_list_row_get(data->list));
  errands_sidebar_update_filter_rows();
  LOG("Add task '%s' to task list '%s'", errands_data_get_uid(data->ical), list_uid);
  errands_sync_create_task(data);
  errands_task_list_reload(self, false);
}

static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry) {
  search_query = gtk_editable_get_text(GTK_EDITABLE(entry));
  LOG("Search query changed to '%s'", search_query);
  errands_task_list_reload(self, false);
}

// static void on_date_btn_clicked_cb(ErrandsTaskList *self, GtkButton *btn) {
//   const char *text = gtk_editable_get_text(GTK_EDITABLE(self->entry));
//   if (!text || STR_EQUAL(text, "")) return;
//   // errands_task_list_date_dialog_show(NULL);
// }
