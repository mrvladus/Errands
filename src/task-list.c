#include "task-list.h"
#include "data.h"
#include "gtk/gtk.h"
#include "settings.h"
#include "sidebar.h"
#include "sync.h"
#include "task-item.h"
#include "task-menu.h"
#include "task.h"
#include "utils.h"

#include <glib/gi18n.h>

static const char *search_query = NULL;

static ErrandsTask *entry_task = NULL;
static TaskData *entry_task_data = NULL;

static void on_setup_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item);
static void on_bind_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item);
static void on_unbind_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item);

static void on_task_list_entry_activated_cb(ErrandsTaskList *self);
static void on_task_list_entry_text_changed_cb(ErrandsTaskList *self);
static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry);
static void on_motion_cb(GtkEventControllerMotion *ctrl, gdouble x, gdouble y, ErrandsTaskList *self);
static void on_listview_activate_cb(GtkListView *list_view, guint position);

static void on_focus_entry_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_entry_task_menu_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);

static bool __task_today_parent_match_func(TaskData *data);
static bool __task_today_child_match_func(TaskData *data);
static bool __task_or_descendants_match_search_query(TaskData *data, const char *query);
static bool __task_ancestor_match_search_query(TaskData *data, const char *query);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)

static void errands_task_list_dispose(GObject *gobject) {
  if (entry_task) g_object_run_dispose(G_OBJECT(entry_task));
  errands_task_data_free(entry_task_data);

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
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, task_menu);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, motion_ctrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, scrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, list_view);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_sort_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_text_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_search_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_motion_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_listview_activate_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_setup_item_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_bind_item_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_unbind_item_cb);
}

gboolean filter_func(GtkTreeListRow *row, ErrandsTaskList *self) {
  ErrandsTaskItem *item = ERRANDS_TASK_ITEM(gtk_tree_list_row_get_item(row));
  TaskData *data = errands_task_item_get_data(item);
  bool result = false;

  if (search_query && *search_query) {
    // If searching, show tasks that match or have matching descendants
    result = __task_or_descendants_match_search_query(data, search_query) ||
             __task_ancestor_match_search_query(data, search_query);
  } else {
    // Normal page-based filtering
    switch (self->page) {
    case ERRANDS_TASK_LIST_PAGE_TODAY: {
      result = __task_today_parent_match_func(data) || __task_today_child_match_func(data);
    } break;
    case ERRANDS_TASK_LIST_PAGE_ALL: result = true; break;
    case ERRANDS_TASK_LIST_PAGE_TASK_LIST: result = data->list == self->data; break;
    }
  }

  return result;
}

static int sort_func(ErrandsTaskItem *a, ErrandsTaskItem *b, ErrandsTaskList *self) {
  TaskData *td_a = errands_task_item_get_data(a);
  TaskData *td_b = errands_task_item_get_data(b);

  // Cancelled
  gboolean cancelled_a = errands_data_get_cancelled(td_a->ical);
  gboolean cancelled_b = errands_data_get_cancelled(td_b->ical);
  if (cancelled_a != cancelled_b) return cancelled_a - cancelled_b;

  // Completed
  gboolean completed_a = errands_data_is_completed(td_a->ical);
  gboolean completed_b = errands_data_is_completed(td_b->ical);
  if (completed_a != completed_b) return completed_a - completed_b;

  bool asc_order = errands_settings_get(SETTING_SORT_ORDER).i;
  switch (errands_settings_get(SETTING_SORT_BY).i) {
  case SORT_TYPE_CREATION_DATE: {
    icaltimetype creation_date_a = errands_data_get_created(asc_order ? td_b->ical : td_a->ical);
    icaltimetype creation_date_b = errands_data_get_created(asc_order ? td_a->ical : td_b->ical);
    return icaltime_compare(creation_date_b, creation_date_a);
  }
  case SORT_TYPE_DUE_DATE: {
    icaltimetype due_a = errands_data_get_due(asc_order ? td_b->ical : td_a->ical);
    icaltimetype due_b = errands_data_get_due(asc_order ? td_a->ical : td_b->ical);
    bool null_a = icaltime_is_null_time(due_a);
    bool null_b = icaltime_is_null_time(due_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(due_a, due_b);
  }
  case SORT_TYPE_PRIORITY: {
    int p_a = errands_data_get_priority(asc_order ? td_b->ical : td_a->ical);
    int p_b = errands_data_get_priority(asc_order ? td_a->ical : td_b->ical);
    return p_b - p_a;
  }
  case SORT_TYPE_START_DATE: {
    icaltimetype start_a = errands_data_get_start(asc_order ? td_b->ical : td_a->ical);
    icaltimetype start_b = errands_data_get_start(asc_order ? td_a->ical : td_b->ical);
    bool null_a = icaltime_is_null_time(start_a);
    bool null_b = icaltime_is_null_time(start_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(start_a, start_b);
  }
  default: return 0;
  }
}

static GListModel *task_children_func(gpointer item, gpointer user_data) {
  ErrandsTaskItem *task_item = ERRANDS_TASK_ITEM(item);
  GListModel *model = errands_task_item_get_children_model(task_item);

  if (!model) return NULL;

  return g_object_ref(model);
}

static void errands_task_list_init(ErrandsTaskList *self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  GSimpleActionGroup *ag = errands_add_action_group(self, "task-list");
  errands_add_action(ag, "focus-entry", on_focus_entry_action_cb, self, NULL);
  errands_add_action(ag, "show-entry-task-menu", on_entry_task_menu_action_cb, self, NULL);

  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(self->search_bar), GTK_EDITABLE(self->search_entry));
  // Create entry task
  entry_task = errands_task_new();
  entry_task_data = errands_task_data_create_task(NULL, NULL, "");
  errands_task_set_data(entry_task, entry_task_data);

  self->task_model = g_list_store_new(ERRANDS_TYPE_TASK_ITEM);

  for_range(i, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, i);
    for_range(j, 0, list->children->len) {
      TaskData *data = g_ptr_array_index(list->children, j);
      g_autoptr(ErrandsTaskItem) item = errands_task_item_new(data, NULL);
      g_list_store_append(self->task_model, item);
    }
  }

  GtkTreeListModel *tree_model =
      gtk_tree_list_model_new(G_LIST_MODEL(self->task_model), false, true, task_children_func, NULL, NULL);
  self->tree_sorter =
      gtk_tree_list_row_sorter_new(GTK_SORTER(gtk_custom_sorter_new((GCompareDataFunc)sort_func, self, NULL)));
  GtkSortListModel *sort_model = gtk_sort_list_model_new(G_LIST_MODEL(tree_model), GTK_SORTER(self->tree_sorter));
  self->filter = GTK_FILTER(gtk_custom_filter_new((GtkCustomFilterFunc)filter_func, self, NULL));
  GtkFilterListModel *filter_model = gtk_filter_list_model_new(G_LIST_MODEL(sort_model), self->filter);
  GtkSelectionModel *selection_model = GTK_SELECTION_MODEL(gtk_no_selection_new(G_LIST_MODEL(filter_model)));
  gtk_list_view_set_model(GTK_LIST_VIEW(self->list_view), selection_model);
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

// ---------- PRIVATE FUNCTIONS ---------- //

static bool __task_today_parent_match_func(TaskData *data) {
  for (TaskData *task = data->parent; task; task = task->parent)
    if (errands_data_is_due(task->ical)) return true;
  return false;
}

static bool __task_today_child_match_func(TaskData *data) {
  if (errands_data_is_due(data->ical)) return true;
  for (size_t i = 0; i < data->children->len; ++i)
    if (__task_today_child_match_func(g_ptr_array_index(data->children, i))) return true;

  return false;
}

static bool __task_match_search_query(TaskData *data, const char *query) {
  if (!query || !*query) return false;
  g_autofree char *folded_query = g_utf8_casefold(query, -1);
  const char *text = errands_data_get_text(data->ical);
  if (text) {
    g_autofree char *folded_text = g_utf8_casefold(text, -1);
    if (g_strstr_len(folded_text, -1, folded_query)) return true;
  }
  const char *notes = errands_data_get_notes(data->ical);
  if (notes) {
    g_autofree char *folded_notes = g_utf8_casefold(notes, -1);
    if (g_strstr_len(folded_notes, -1, folded_query)) return true;
  }
  g_auto(GStrv) tags = errands_data_get_tags(data->ical);
  if (tags) for_range(i, 0, g_strv_length(tags)) {
      g_autofree char *folded_tag = g_utf8_casefold(tags[i], -1);
      if (g_strstr_len(folded_tag, -1, folded_query)) return true;
    }

  return false;
}

static bool __task_or_descendants_match_search_query(TaskData *data, const char *query) {
  if (__task_match_search_query(data, query)) return true;
  for (guint i = 0; i < data->children->len; i++) {
    TaskData *child = g_ptr_array_index(data->children, i);
    if (__task_or_descendants_match_search_query(child, query)) return true;
  }

  return false;
}

static bool __task_ancestor_match_search_query(TaskData *data, const char *query) {
  TaskData *parent = data->parent;
  while (parent) {
    if (__task_match_search_query(parent, query)) return true;
    parent = parent->parent;
  }

  return false;
}

static void __expand_all_visible_rows_idle_cb(GtkTreeListRow *row) { gtk_tree_list_row_set_expanded(row, true); }

static void __expand_all_visible_rows(ErrandsTaskList *self) {
  GListModel *model = G_LIST_MODEL(gtk_list_view_get_model(GTK_LIST_VIEW(self->list_view)));
  bool expanded_any = false;
  for (guint i = 0; i < g_list_model_get_n_items(model); i++) {
    g_autoptr(GObject) obj = g_list_model_get_item(model, i);
    if (GTK_IS_TREE_LIST_ROW(obj)) {
      GtkTreeListRow *row = GTK_TREE_LIST_ROW(obj);
      if (gtk_tree_list_row_is_expandable(row) && !gtk_tree_list_row_get_expanded(row)) {
        g_idle_add_once((GSourceOnceFunc)__expand_all_visible_rows_idle_cb, row);
        expanded_any = true;
      }
    }
  }
  // If we expanded something, schedule another check to expand newly visible rows
  if (expanded_any) g_idle_add_once((GSourceOnceFunc)__expand_all_visible_rows, self);
}

// ---------- TASKS RECYCLER ---------- //

static void on_setup_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item) {
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_tree_expander_new());
  gtk_tree_expander_set_child(expander, GTK_WIDGET(errands_task_new()));
  gtk_list_item_set_child(list_item, GTK_WIDGET(expander));
  gtk_list_item_set_focusable(list_item, true);
}

static void on_bind_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item) {
  GtkTreeListRow *row = gtk_list_item_get_item(list_item);
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(list_item));
  gtk_tree_expander_set_list_row(expander, row);
  ErrandsTask *task = ERRANDS_TASK(gtk_tree_expander_get_child(expander));
  ErrandsTaskItem *item = gtk_tree_list_row_get_item(row);
  g_object_set(task, "task-item", item, NULL);
  g_object_set(item, "task-widget", task, NULL);
  g_object_bind_property(item, "children-model-is-empty", expander, "hide-expander", G_BINDING_SYNC_CREATE);
  task->row = row;
}

static void on_unbind_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item) {
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(list_item));
  ErrandsTask *task = ERRANDS_TASK(gtk_tree_expander_get_child(expander));
  task->row = NULL;
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
  case ERRANDS_TASK_LIST_PAGE_ALL: {
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("All Tasks"));
    size_t total = 0, completed = 0;
    for_range(i, 0, errands_data_lists->len) {
      ListData *list = g_ptr_array_index(errands_data_lists, i);
      CONTINUE_IF(errands_data_get_deleted(list->ical));
      g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(list);
      for_range(j, 0, tasks->len) {
        icalcomponent *ical = g_ptr_array_index(tasks, j);
        CONTINUE_IF(errands_data_get_deleted(ical) || errands_data_get_cancelled(ical));
        total++;
        if (errands_data_is_completed(ical)) completed++;
      }
    }
    gtk_widget_set_visible(self->scrl, total > 0);
  } break;
  case ERRANDS_TASK_LIST_PAGE_TODAY: {
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("Tasks for Today"));
    size_t total = 0, completed = 0;
    for_range(i, 0, errands_data_lists->len) {
      ListData *list = g_ptr_array_index(errands_data_lists, i);
      CONTINUE_IF(errands_data_get_deleted(list->ical));
      g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(list);
      for_range(j, 0, tasks->len) {
        icalcomponent *ical = g_ptr_array_index(tasks, j);
        CONTINUE_IF(errands_data_get_deleted(ical) || errands_data_get_cancelled(ical));
        if (errands_data_is_due(ical)) {
          total++;
          if (errands_data_is_completed(ical)) completed++;
        }
      }
    }
    gtk_widget_set_visible(self->scrl, total > 0);
  } break;
  case ERRANDS_TASK_LIST_PAGE_TASK_LIST: {
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), errands_data_get_list_name(self->data->ical));
    size_t total = 0, completed = 0;
    for_range(i, 0, errands_data_lists->len) {
      ListData *list = g_ptr_array_index(errands_data_lists, i);
      CONTINUE_IF(errands_data_get_deleted(list->ical));
      g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(list);
      for_range(j, 0, tasks->len) {
        icalcomponent *ical = g_ptr_array_index(tasks, j);
        CONTINUE_IF(errands_data_get_deleted(ical) || errands_data_get_cancelled(ical));
        total++;
        if (errands_data_is_completed(ical)) completed++;
      }
    }
    gtk_widget_set_visible(self->scrl, total > 0);
  } break;
  }
}

typedef struct {
  ErrandsTaskList *self;
  GtkFilterChange change;
} _FilterCallbackData;

static _FilterCallbackData *__filter_cb_data_new(ErrandsTaskList *self, GtkFilterChange change) {
  _FilterCallbackData *cb_data = g_new0(_FilterCallbackData, 1);
  cb_data->self = self;
  cb_data->change = change;

  return cb_data;
}

static void __filter_cb(_FilterCallbackData *cb_data) {
  gtk_filter_changed(cb_data->self->filter, cb_data->change);
  g_free(cb_data);
}

void errands_task_list_show_today_tasks(ErrandsTaskList *self) {
  LOG("Task List: Show today tasks");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_TODAY;
  errands_task_list_update_title(self);
  gtk_widget_set_visible(self->entry_box, false);
  g_idle_add_once((GSourceOnceFunc)__filter_cb, __filter_cb_data_new(self, GTK_FILTER_CHANGE_DIFFERENT));
  __expand_all_visible_rows(self);
}

void errands_task_list_show_all_tasks(ErrandsTaskList *self) {
  LOG("Task List: Show all tasks");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_ALL;
  gtk_widget_set_visible(self->entry_box, false);
  errands_task_list_update_title(self);
  g_idle_add_once((GSourceOnceFunc)__filter_cb, __filter_cb_data_new(self, GTK_FILTER_CHANGE_LESS_STRICT));
}

void errands_task_list_show_task_list(ErrandsTaskList *self, ListData *data) {
  self->data = data;
  self->page = ERRANDS_TASK_LIST_PAGE_TASK_LIST;
  gtk_widget_set_visible(self->entry_box, true);
  errands_task_list_update_title(self);
  g_idle_add_once((GSourceOnceFunc)__filter_cb, __filter_cb_data_new(self, GTK_FILTER_CHANGE_DIFFERENT));
}

void errands_task_list_sort(ErrandsTaskList *self, GtkSorterChange change) {
  gtk_sorter_changed(GTK_SORTER(self->tree_sorter), change);
}

void errands_task_list_filter(ErrandsTaskList *self, GtkFilterChange change) {
  g_idle_add_once((GSourceOnceFunc)__filter_cb, __filter_cb_data_new(self, change));
}

static void __remove_deleted_tasks(ErrandsTaskList *self, GListStore *model) {
  for_range(i, 0, g_list_model_get_n_items(G_LIST_MODEL(model))) {
    g_autoptr(ErrandsTaskItem) child = g_list_model_get_item(G_LIST_MODEL(model), i);
    __remove_deleted_tasks(self, G_LIST_STORE(errands_task_item_get_children_model(child)));
    TaskData *data = errands_task_item_get_data(child);
    if (errands_data_get_deleted(data->ical)) g_list_store_remove(model, i--);
  }
}

void errands_task_list_delete_completed(ErrandsTaskList *self, ListData *list) {
  if (!self->data) return;
  g_autoptr(GPtrArray) tasks = g_ptr_array_sized_new(list->children->len);
  errands_list_data_get_flat_list(list, tasks);
  bool deleted = false;
  for_range(i, 0, tasks->len) {
    TaskData *task = g_ptr_array_index(tasks, i);
    if (errands_data_is_completed(task->ical) && !errands_data_get_deleted(task->ical)) {
      errands_data_set_deleted(task->ical, true);
      errands_sync_delete_task(task);
      deleted = true;
    }
  }
  if (deleted) errands_list_data_save(list);
  __remove_deleted_tasks(self, self->task_model);
}

void errands_task_list_delete_cancelled(ErrandsTaskList *self, ListData *list) {
  if (!self->data) return;
  g_autoptr(GPtrArray) tasks = g_ptr_array_sized_new(list->children->len);
  errands_list_data_get_flat_list(list, tasks);
  bool deleted = false;
  for_range(i, 0, tasks->len) {
    TaskData *task = g_ptr_array_index(tasks, i);
    if (errands_data_get_cancelled(task->ical) && !errands_data_get_deleted(task->ical)) {
      errands_data_set_deleted(task->ical, true);
      errands_sync_delete_task(task);
      deleted = true;
    }
  }
  if (deleted) errands_list_data_save(list);
  __remove_deleted_tasks(self, self->task_model);
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
  g_list_store_append(self->task_model, errands_task_item_new(data, NULL));
  // Reload entry task
  entry_task->data->ical = icalcomponent_new(ICAL_VTODO_COMPONENT);
  // Reset text
  g_object_set(self->entry, "text", "", NULL);
  // Update UI
  errands_sidebar_task_list_row_update(errands_sidebar_task_list_row_get(data->list));
  errands_sidebar_update_filter_rows();
  LOG("Add task '%s' to task list '%s'", errands_data_get_uid(data->ical), list_uid);
  errands_sync_create_task(data);
  errands_task_list_update_title(self);

  gtk_list_view_scroll_to(GTK_LIST_VIEW(self->list_view), 0, 0, NULL);
}

static void on_task_list_entry_text_changed_cb(ErrandsTaskList *self) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(self->entry));
  gtk_widget_set_sensitive(self->entry_apply_btn, text && !STR_EQUAL(text, ""));
}

static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry) {
  search_query = gtk_editable_get_text(GTK_EDITABLE(entry));
  LOG("Search query changed to '%s'", search_query);
  gtk_filter_changed(self->filter, GTK_FILTER_CHANGE_DIFFERENT);
  if (search_query && *search_query) __expand_all_visible_rows(self);
}

static void on_motion_cb(GtkEventControllerMotion *ctrl, gdouble x, gdouble y, ErrandsTaskList *self) {
  self->x = x;
  self->y = y;
}

static void on_listview_activate_cb(GtkListView *list_view, guint position) {
  g_autoptr(GtkTreeListRow) row = g_list_model_get_item(G_LIST_MODEL(gtk_list_view_get_model(list_view)), position);
  if (!row) return;
  if (GTK_IS_TREE_LIST_ROW(row)) gtk_tree_list_row_set_expanded(row, !gtk_tree_list_row_get_expanded(row));
}
