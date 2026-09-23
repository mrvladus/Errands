#include "task-list.h"
#include "data.h"
#include "delete-list-dialog.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "rename-list-dialog.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
// #include "sync.h"
#include "task-item.h"
#include "task-list-item.h"
#include "task.h"
#include "utils.h"
#include "window.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static const char *search_query = NULL;

static void on_setup_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item);
static void on_bind_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item);
static void on_unbind_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item);
// static void on_header_setup_item_cb(GtkSignalListItemFactory *self, GtkListHeader *header);
// static void on_header_bind_item_cb(GtkSignalListItemFactory *self, GtkListHeader *header);

static void on_task_list_entry_activated_cb(ErrandsTaskList *self);
static void on_task_list_entry_text_changed_cb(ErrandsTaskList *self);
static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry);
static void on_listview_activate_cb(GtkListView *list_view, guint position);

static void on_focus_entry_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_export_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_print_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_rename_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_delete_completed_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_delete_cancelled_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_delete_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_show_completed_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_show_cancelled_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_sort_order_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_action_sort_by_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);

static bool today_filter_func(ErrandsTaskItem *item, ErrandsTaskList *self);
static bool toplevel_filter_func(ErrandsTaskItem *item, ErrandsTaskList *self);
static bool tree_filter_func(GtkTreeListRow *row, ErrandsTaskList *self);
static int __sort_func(ErrandsTaskItem *a, ErrandsTaskItem *b, ErrandsTaskList *self);
static bool task_today_parent_match_func(ErrandsTaskItem *item);
// static bool __task_today_child_match_func(TaskData *data);
// static bool __task_or_descendants_match_search_query(TaskData *data, const char *query);
// static bool __task_ancestor_match_search_query(TaskData *data, const char *query);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)

static void errands_task_list_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST);
  G_OBJECT_CLASS(errands_task_list_parent_class)->dispose(gobject);
}

static void errands_task_list_class_init(ErrandsTaskListClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_dispose;

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/task-list.ui");

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, title);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, search_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, menu_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, search_bar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, search_entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, entry_apply_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, scrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, list_view);

  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_text_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_search_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_listview_activate_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_setup_item_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_bind_item_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_unbind_item_cb);
  // gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_header_setup_item_cb);
  // gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_header_bind_item_cb);
}

static GListModel *task_children_func(gpointer item, gpointer user_data) {
  return g_object_ref(errands_task_item_get_children_model(ERRANDS_TASK_ITEM(item)));
}

static void errands_task_list_init(ErrandsTaskList *self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  GSimpleActionGroup *ag = errands_add_action_group(self, "task-list");
  errands_add_action(ag, "focus-entry", on_focus_entry_action_cb, self, NULL);
  errands_add_action(ag, "rename", on_action_rename_cb, self, NULL);
  errands_add_action(ag, "print", on_action_print_cb, self, NULL);
  errands_add_action(ag, "export", on_action_export_cb, self, NULL);
  errands_add_action(ag, "delete-completed", on_action_delete_completed_cb, self, NULL);
  errands_add_action(ag, "delete-cancelled", on_action_delete_cancelled_cb, self, NULL);
  errands_add_action(ag, "delete", on_action_delete_cb, self, NULL);
  errands_add_stateful_action(ag, "show-completed", NULL,
                              g_variant_new_boolean(errands_settings_get(SETTING_SHOW_COMPLETED).b),
                              on_action_show_completed_cb, self);
  errands_add_stateful_action(ag, "show-cancelled", NULL,
                              g_variant_new_boolean(errands_settings_get(SETTING_SHOW_CANCELLED).b),
                              on_action_show_cancelled_cb, self);
  errands_add_stateful_action(ag, "sort-order", G_VARIANT_TYPE_STRING,
                              g_variant_new_string(errands_settings_get(SETTING_SORT_ORDER).s == 0 ? "desc" : "asc"),
                              on_action_sort_order_cb, self);
  const char *sort_by_str = NULL;
  switch (errands_settings_get(SETTING_SORT_BY).i) {
  case SORT_TYPE_CREATION_DATE: sort_by_str = "created"; break;
  case SORT_TYPE_START_DATE: sort_by_str = "start"; break;
  case SORT_TYPE_DUE_DATE: sort_by_str = "due"; break;
  case SORT_TYPE_PRIORITY: sort_by_str = "priority"; break;
  }
  errands_add_stateful_action(ag, "sort-by", G_VARIANT_TYPE_STRING, g_variant_new_string(sort_by_str),
                              on_action_sort_by_cb, self);

  gtk_search_bar_connect_entry(GTK_SEARCH_BAR(self->search_bar), GTK_EDITABLE(self->search_entry));

  self->toplevel_tasks_models = g_list_store_new(G_TYPE_LIST_MODEL);
  for_range(i, 0, g_list_model_get_n_items(G_LIST_MODEL(task_lists_model))) {
    g_autoptr(ErrandsTaskListItem) item = g_list_model_get_item(G_LIST_MODEL(task_lists_model), i);
    g_list_store_append(self->toplevel_tasks_models, item->tasks);
  }
  self->all_tasks_model = gtk_flatten_list_model_new(G_LIST_MODEL(self->toplevel_tasks_models));
  self->current_model = gtk_filter_list_model_new(NULL, NULL);
  self->tree_model =
      gtk_tree_list_model_new(G_LIST_MODEL(self->current_model), false, true, task_children_func, NULL, NULL);
  self->tree_sorter =
      gtk_tree_list_row_sorter_new(GTK_SORTER(gtk_custom_sorter_new((GCompareDataFunc)__sort_func, self, NULL)));
  GtkSortListModel *sort_model = gtk_sort_list_model_new(G_LIST_MODEL(self->tree_model), GTK_SORTER(self->tree_sorter));
  self->tree_filter = GTK_FILTER(gtk_custom_filter_new((GtkCustomFilterFunc)tree_filter_func, self, NULL));
  GtkFilterListModel *filter_model = gtk_filter_list_model_new(G_LIST_MODEL(sort_model), self->tree_filter);

  gtk_list_view_set_model(GTK_LIST_VIEW(self->list_view),
                          GTK_SELECTION_MODEL(gtk_no_selection_new(G_LIST_MODEL(filter_model))));
}

ErrandsTaskList *errands_task_list_new() { return g_object_new(ERRANDS_TYPE_TASK_LIST, NULL); }

// ---------- PRIVATE FUNCTIONS ---------- //

static bool tree_filter_func(GtkTreeListRow *row, ErrandsTaskList *self) {
  g_autoptr(ErrandsTaskItem) item = gtk_tree_list_row_get_item(row);
  if (!errands_settings_get(SETTING_SHOW_COMPLETED).b && errands_task_item_get_completed(item)) return false;
  if (!errands_settings_get(SETTING_SHOW_CANCELLED).b && errands_task_item_get_cancelled(item)) return false;
  bool result = false;
  switch (self->page) {
  case ERRANDS_TASK_LIST_PAGE_TASK_LIST: {
    result = self->item == errands_task_item_get_list(item);
    break;
  }
  case ERRANDS_TASK_LIST_PAGE_ALL: result = true; break;
  case ERRANDS_TASK_LIST_PAGE_TODAY:
    result = (errands_task_item_is_due(item) || task_today_parent_match_func(item));
    break;
  }
  return result;
}

static int __sort_func(ErrandsTaskItem *a, ErrandsTaskItem *b, ErrandsTaskList *self) {
  icalcomponent *td_a = errands_task_item_get_ical(a);
  icalcomponent *td_b = errands_task_item_get_ical(b);

  // Cancelled
  gboolean cancelled_a = errands_data_get_cancelled(td_a);
  gboolean cancelled_b = errands_data_get_cancelled(td_b);
  if (cancelled_a != cancelled_b) return cancelled_a - cancelled_b;

  // Completed
  gboolean completed_a = errands_data_is_completed(td_a);
  gboolean completed_b = errands_data_is_completed(td_b);
  if (completed_a != completed_b) return completed_a - completed_b;

  bool asc_order = errands_settings_get(SETTING_SORT_ORDER).i;
  switch (errands_settings_get(SETTING_SORT_BY).i) {
  case SORT_TYPE_CREATION_DATE: {
    icaltimetype creation_date_a = errands_data_get_created(asc_order ? td_b : td_a);
    icaltimetype creation_date_b = errands_data_get_created(asc_order ? td_a : td_b);
    return icaltime_compare(creation_date_b, creation_date_a);
  }
  case SORT_TYPE_DUE_DATE: {
    icaltimetype due_a = errands_data_get_due(asc_order ? td_b : td_a);
    icaltimetype due_b = errands_data_get_due(asc_order ? td_a : td_b);
    bool null_a = icaltime_is_null_time(due_a);
    bool null_b = icaltime_is_null_time(due_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(due_a, due_b);
  }
  case SORT_TYPE_PRIORITY: {
    int p_a = errands_data_get_priority(asc_order ? td_b : td_a);
    int p_b = errands_data_get_priority(asc_order ? td_a : td_b);
    return p_b - p_a;
  }
  case SORT_TYPE_START_DATE: {
    icaltimetype start_a = errands_data_get_start(asc_order ? td_b : td_a);
    icaltimetype start_b = errands_data_get_start(asc_order ? td_a : td_b);
    bool null_a = icaltime_is_null_time(start_a);
    bool null_b = icaltime_is_null_time(start_b);
    if (null_a != null_b) return null_a - null_b;
    return icaltime_compare(start_a, start_b);
  }
  default: return 0;
  }
}

static bool task_today_parent_match_func(ErrandsTaskItem *item) {
  for (ErrandsTaskItem *parent = errands_task_item_get_parent(item); parent;
       parent = errands_task_item_get_parent(parent))
    if (errands_task_item_is_due(parent)) return true;
  return false;
}

// static bool __task_today_child_match_func(TaskData *data) {
//   for (size_t i = 0; i < data->children->len; ++i)
//     if (__task_today_child_match_func(g_ptr_array_index(data->children, i))) return true;
//   return errands_data_is_due(data->ical);
// }

// static bool __task_match_search_query(TaskData *data, const char *query) {
//   if (!query || !*query) return false;
//   g_autofree char *folded_query = g_utf8_casefold(query, -1);
//   const char *text = errands_data_get_text(data->ical);
//   if (text) {
//     g_autofree char *folded_text = g_utf8_casefold(text, -1);
//     if (g_strstr_len(folded_text, -1, folded_query)) return true;
//   }
//   const char *notes = errands_data_get_notes(data->ical);
//   if (notes) {
//     g_autofree char *folded_notes = g_utf8_casefold(notes, -1);
//     if (g_strstr_len(folded_notes, -1, folded_query)) return true;
//   }
//   g_auto(GStrv) tags = errands_data_get_tags(data->ical);
//   if (tags) for_range(i, 0, g_strv_length(tags)) {
//       g_autofree char *folded_tag = g_utf8_casefold(tags[i], -1);
//       if (g_strstr_len(folded_tag, -1, folded_query)) return true;
//     }

//   return false;
// }

// static bool __task_or_descendants_match_search_query(TaskData *data, const char *query) {
//   if (__task_match_search_query(data, query)) return true;
//   for (guint i = 0; i < data->children->len; i++) {
//     TaskData *child = g_ptr_array_index(data->children, i);
//     if (__task_or_descendants_match_search_query(child, query)) return true;
//   }

//   return false;
// }

// static bool __task_ancestor_match_search_query(TaskData *data, const char *query) {
//   TaskData *parent = data->parent;
//   while (parent) {
//     if (__task_match_search_query(parent, query)) return true;
//     parent = parent->parent;
//   }

//   return false;
// }

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

// ---------- TASKS LIST ---------- //

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

  g_object_set(item, "task-widget", task, NULL);
  g_object_set(task, "item", item, NULL);

  g_object_bind_property(item, "color", task, "color", G_BINDING_SYNC_CREATE);
  g_object_bind_property(item, "priority", task, "priority", G_BINDING_SYNC_CREATE);
  g_object_bind_property(item, "notes", task, "notes", G_BINDING_SYNC_CREATE);
  g_object_bind_property(item, "dtstart", task, "dtstart", G_BINDING_SYNC_CREATE);
  g_object_bind_property(item, "dtend", task, "dtend", G_BINDING_SYNC_CREATE);
  g_object_bind_property(item, "tags", task, "tags", G_BINDING_SYNC_CREATE);
  g_object_bind_property(item, "attachments", task, "attachments", G_BINDING_SYNC_CREATE);
  g_object_bind_property(item, "rrule", task, "rrule", G_BINDING_SYNC_CREATE);
  g_object_bind_property(item, "completed", task->complete_btn, "active",
                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  g_object_bind_property(item, "has-no-children", expander, "hide-expander", G_BINDING_SYNC_CREATE);
}

static void on_unbind_item_cb(GtkSignalListItemFactory *self, GtkListItem *list_item) {
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(list_item));
  ErrandsTask *task = ERRANDS_TASK(gtk_tree_expander_get_child(expander));
  g_object_set(task->item, "task-widget", NULL, NULL);
}

// ---------- ACTIONS ---------- //

static void on_focus_entry_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  gtk_widget_grab_focus(self->entry);
}

static void on_action_export_finish_cb(GObject *obj, GAsyncResult *res, ErrandsTaskListItem *item) {
  g_autoptr(GFile) f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!f) return;
  g_autofree char *path = g_file_get_path(f);
  FILE *file = fopen(path, "w");
  if (!file) {
    errands_window_add_toast(_("Export failed"), 2);
    return;
  }
  autofree char *ical = icalcomponent_as_ical_string(item->ical);
  fprintf(file, "%s", ical);
  fclose(file);
  errands_window_add_toast(_("Exported"), 1);
  g_message("Export task list %s", item->uid);
}

static void on_action_export_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  const char *filename = tmp_str_printf("%s.ics", self->item->uid);
  g_object_set(dialog, "initial-name", filename, NULL);
  gtk_file_dialog_save(dialog, GTK_WINDOW(state.main_window), NULL, (GAsyncReadyCallback)on_action_export_finish_cb,
                       self->item);
}

static void on_action_rename_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  errands_rename_list_dialog_show(errands_sidebar_find_list(self->item->uid));
}

static void on_action_delete_completed_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  if (!self->item) return;
  int deleted_n = errands_task_list_item_delete_completed(self->item);
  if (deleted_n == 0) {
    errands_window_add_toast(tmp_str_printf(_("No tasks to delete"), deleted_n), 2);
    return;
  }
  errands_task_list_item_remove_deleted_tasks(self->item);
  errands_task_list_filter(self, GTK_FILTER_CHANGE_DIFFERENT);
  errands_window_add_toast(tmp_str_printf(_("Deleted %zu tasks"), deleted_n), 2);
}

static void on_action_delete_cancelled_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  if (!self->item) return;
  int deleted_n = errands_task_list_item_delete_cancelled(self->item);
  if (deleted_n == 0) {
    errands_window_add_toast(tmp_str_printf(_("No tasks to delete"), deleted_n), 2);
    return;
  }
  errands_task_list_item_remove_deleted_tasks(self->item);
  errands_task_list_filter(self, GTK_FILTER_CHANGE_DIFFERENT);
  errands_window_add_toast(tmp_str_printf(_("Deleted %zu tasks"), deleted_n), 2);
}

static void on_action_delete_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  errands_delete_list_dialog_show(errands_sidebar_find_list(self->item->uid));
}

static void on_action_show_completed_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  gboolean state = g_variant_get_boolean(param);
  g_simple_action_set_state(action, param);
  errands_settings_set(SETTING_SHOW_COMPLETED, &state);
  errands_task_list_filter(self, GTK_FILTER_CHANGE_DIFFERENT);
  TODO("call errands_task_item_update() on every task list item to show/hide expander");
}

static void on_action_show_cancelled_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  gboolean state = g_variant_get_boolean(param);
  g_simple_action_set_state(action, param);
  errands_settings_set(SETTING_SHOW_CANCELLED, &state);
  errands_task_list_filter(self, state ? GTK_FILTER_CHANGE_LESS_STRICT : GTK_FILTER_CHANGE_MORE_STRICT);
  TODO("call errands_task_item_update() on every task list item to show/hide expander");
}

static void on_action_sort_order_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  g_simple_action_set_state(action, param);
  const char *new = g_variant_get_string(param, NULL);
  ErrandsSettingSortOrder order = g_str_equal(new, "desc") ? SORT_ORDER_DESC : SORT_ORDER_ASC;
  errands_settings_set(SETTING_SORT_ORDER, &order);
  errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_INVERTED);
  g_message("Task List: Set sort order: %s", new);
}

static void on_action_sort_by_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  g_simple_action_set_state(action, param);
  const char *new = g_variant_get_string(param, NULL);
  ErrandsSettingSortType by = SORT_TYPE_CREATION_DATE;
  if (g_str_equal(new, "created")) by = SORT_TYPE_CREATION_DATE;
  else if (g_str_equal(new, "start")) by = SORT_TYPE_START_DATE;
  else if (g_str_equal(new, "due")) by = SORT_TYPE_DUE_DATE;
  else if (g_str_equal(new, "priority")) by = SORT_TYPE_PRIORITY;
  errands_settings_set(SETTING_SORT_BY, &by);
  errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_DIFFERENT);
}

// - PRINTING - //

#define FONT_SIZE      12
#define LINE_HEIGHT    20
#define LINES_PER_PAGE 40

// Function to calculate number of pages and handle pagination
static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context, const char *text) {
  size_t num_lines = 0;
  char c;
  size_t len = 0;
  for (size_t i = 0; i < strlen(text); i++) {
    c = text[i];
    if (c == '\n') {
      num_lines++;
      len = 0;
      continue;
    }
    if (len > 73 && c != '\n') {
      num_lines++;
      len = 0;
    }
    len++;
  }
  const size_t total_pages = (num_lines + LINES_PER_PAGE - 1) / LINES_PER_PAGE;
  gtk_print_operation_set_n_pages(operation, total_pages);
}

// Function to draw the text, handling LINES_PER_PAGE
static void print_draw_page(GtkPrintOperation *operation, GtkPrintContext *context, int page_nr, gpointer user_data) {
  cairo_t *cr = gtk_print_context_get_cairo_context(context);
  const char *text = (const char *)user_data;
  // Set the font to monospace and size to 12
  cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, FONT_SIZE);
  // Split the text into lines
  g_autofree gchar *text_copy = g_strdup(text); // Duplicate text to safely tokenize
  char *line = strtok(text_copy, "\n");
  // Calculate the start and end lines for the current page
  size_t start_line = page_nr * LINES_PER_PAGE;
  size_t end_line = start_line + LINES_PER_PAGE;

  const uint8_t x = 10; // Starting x position
  double y = 10;        // Starting y position for the first line

  // Iterate over each line and draw it if it belongs to the current page
  size_t current_line = 0;
  while (line) {
    if (current_line >= start_line && current_line < end_line) {
      cairo_move_to(cr, x, y);   // Move to the next line's position
      cairo_show_text(cr, line); // Draw the current line
      y += LINE_HEIGHT;          // Move down for the next line (LINE_HEIGHT between
                                 // lines)
    }
    current_line++;
    line = strtok(NULL, "\n"); // Get the next line
  }
  // Ensure everything is drawn
  cairo_stroke(cr);
}

void start_print(const char *str) {
  // Create a new print operation
  g_autoptr(GtkPrintOperation) print = gtk_print_operation_new();
  // Connect signal to draw on page
  g_signal_connect(print, "draw-page", G_CALLBACK(print_draw_page), (gpointer)str);
  g_signal_connect(print, "begin-print", G_CALLBACK(begin_print), (gpointer)str);
  // Set default print settings if needed
  GtkPrintOperationResult result =
      gtk_print_operation_run(print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, GTK_WINDOW(state.main_window), NULL);
  // Check the result (if user cancels or accepts the dialog)
  if (result == GTK_PRINT_OPERATION_RESULT_ERROR) g_print("An error occurred during the print operation.\n");
  else if (result == GTK_PRINT_OPERATION_RESULT_APPLY) g_print("Print operation successful.\n");
}

static void on_action_print_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self) {
  g_message("Start printing of the list '%s'", self->item->uid);
  TODO("PRINT");
  // g_autofree gchar *str = list_data_print(row->data);
  // start_print(str);
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_update(ErrandsTaskList *self) {
  bool show_completed = errands_settings_get(SETTING_SHOW_COMPLETED).b;
  bool show_cancelled = errands_settings_get(SETTING_SHOW_CANCELLED).b;
  switch (self->page) {
  case ERRANDS_TASK_LIST_PAGE_ALL: {
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("All Tasks"));
    guint total = 0;
    GListModel *model = G_LIST_MODEL(self->all_tasks_model);
    for_range(i, 0, g_list_model_get_n_items(model)) {
      g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
      if (errands_task_item_get_completed(item) && !show_completed) continue;
      if (errands_task_item_get_cancelled(item) && !show_cancelled) continue;
      total++;
    }
    gtk_widget_set_visible(self->scrl, total > 0);
  } break;
  case ERRANDS_TASK_LIST_PAGE_TODAY: {
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), _("Tasks for Today"));
    // guint total = 0;
    // GListModel *model = G_LIST_MODEL(self->today_tasks_model);
    // for_range(i, 0, g_list_model_get_n_items(model)) {
    //   g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    //   if (errands_task_item_get_completed(item) && !show_completed) continue;
    //   if (errands_task_item_get_cancelled(item) && !show_cancelled) continue;
    //   if (errands_task_item_is_due(item)) total++;
    // }
    // gtk_widget_set_visible(self->scrl, total > 0);
  } break;
  case ERRANDS_TASK_LIST_PAGE_TASK_LIST: {
    adw_window_title_set_title(ADW_WINDOW_TITLE(self->title), self->item->title);
    // guint total = 0;
    // GListModel *model = G_LIST_MODEL(self->current_model);
    // for_range(i, 0, g_list_model_get_n_items(model)) {
    //   g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    //   if (errands_task_item_get_completed(item) && !show_completed) continue;
    //   if (errands_task_item_get_cancelled(item) && !show_cancelled) continue;
    //   total++;
    // }
    // gtk_widget_set_visible(self->scrl, total > 0);
  } break;
  }
}

typedef struct {
  GtkFilter *filter;
  GtkFilterChange change;
} _FilterCallbackData;

static _FilterCallbackData *__filter_cb_data_new(GtkFilter *filter, GtkFilterChange change) {
  _FilterCallbackData *cb_data = g_new0(_FilterCallbackData, 1);
  cb_data->filter = filter;
  cb_data->change = change;

  return cb_data;
}

static void __filter_cb(_FilterCallbackData *cb_data) {
  gtk_filter_changed(cb_data->filter, cb_data->change);
  g_free(cb_data);
}

void errands_task_list_show_today_tasks(ErrandsTaskList *self) {
  g_message("Task List: Show today tasks");
  self->item = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_TODAY;
  gtk_widget_set_visible(self->entry_box, false);
  gtk_widget_set_visible(self->menu_btn, false);
  errands_task_list_filter(self, GTK_FILTER_CHANGE_DIFFERENT);
  errands_task_list_update(self);
  __expand_all_visible_rows(self);
}

void errands_task_list_show_all_tasks(ErrandsTaskList *self) {
  g_message("Task List: Show all tasks");
  self->item = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_ALL;
  gtk_widget_set_visible(self->entry_box, false);
  gtk_widget_set_visible(self->menu_btn, false);
  gtk_filter_list_model_set_model(self->current_model, G_LIST_MODEL(self->all_tasks_model));
  errands_task_list_update(self);
}

void errands_task_list_show_task_list(ErrandsTaskList *self, ErrandsTaskListItem *item) {
  if (item == self->item) return;
  self->item = item;
  self->page = ERRANDS_TASK_LIST_PAGE_TASK_LIST;
  gtk_widget_set_visible(self->entry_box, true);
  gtk_widget_set_visible(self->menu_btn, true);
  gtk_filter_list_model_set_model(self->current_model, G_LIST_MODEL(item->tasks));
  errands_task_list_update(self);
  g_message("Task List: Show task list %s", item->uid);
}

void errands_task_list_sort(ErrandsTaskList *self, GtkSorterChange change) {
  gtk_sorter_changed(GTK_SORTER(self->tree_sorter), change);
}

void errands_task_list_filter(ErrandsTaskList *self, GtkFilterChange change) {
  g_idle_add_once((GSourceOnceFunc)__filter_cb, __filter_cb_data_new(self->tree_filter, change));
}

// ---------- CALLBACKS ---------- //

// static void on_entry_timeout_cb(GtkWidget *entry) {
//   gtk_widget_set_sensitive(entry, true);
//   gtk_widget_grab_focus(entry);
// }

static void on_task_list_entry_activated_cb(ErrandsTaskList *self) {
  if (!self->item) return;
  // Get text
  const char *text = gtk_editable_get_text(GTK_EDITABLE(self->entry));
  g_autofree gchar *dup = g_strdup(text);
  char *stripped = g_strstrip(dup);

  const char *list_uid = self->item->uid;
  if (STR_EQUAL(stripped, "") || STR_EQUAL(list_uid, "")) return;

  g_autoptr(ErrandsTaskItem) task = (ErrandsTaskItem *)errands_task_list_item_create_task(self->item, NULL, stripped);
  // Reset text
  g_object_set(self->entry, "text", "", NULL);
  // Update UI
  errands_sidebar_task_list_update_counter(list_uid);
  errands_sidebar_update_filter_rows();
  g_message("Add task '%s' to task list '%s'", errands_task_item_get_uid(task), list_uid);
  errands_task_list_update(self);
  gtk_list_view_scroll_to(GTK_LIST_VIEW(self->list_view), 0, 0, NULL);
  // gtk_widget_set_sensitive(self->entry, false);
  // g_timeout_add_once(1050, (GSourceOnceFunc)on_entry_timeout_cb, self->entry);
}

static void on_task_list_entry_text_changed_cb(ErrandsTaskList *self) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(self->entry));
  gtk_widget_set_sensitive(self->entry_apply_btn, text && !STR_EQUAL(text, ""));
}

static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry) {
  search_query = gtk_editable_get_text(GTK_EDITABLE(entry));
  g_message("Search query changed to '%s'", search_query);
  // gtk_filter_changed(self->toplevel_filter, GTK_FILTER_CHANGE_DIFFERENT);
  if (search_query && *search_query) __expand_all_visible_rows(self);
}

static void on_listview_activate_cb(GtkListView *list_view, guint position) {
  g_autoptr(GtkTreeListRow) row = g_list_model_get_item(G_LIST_MODEL(gtk_list_view_get_model(list_view)), position);
  if (!row) return;
  if (GTK_IS_TREE_LIST_ROW(row)) gtk_tree_list_row_set_expanded(row, !gtk_tree_list_row_get_expanded(row));
}
