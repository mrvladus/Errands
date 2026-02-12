#include "task-list.h"
#include "data.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "gtk/gtknoselection.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"
#include "task-item.h"
#include "task-menu.h"
#include "task.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <libical/ical.h>
#include <stddef.h>

static GPtrArray *current_task_list = NULL;
static const char *search_query = NULL;

static ErrandsTask *entry_task = NULL;
static TaskData *entry_task_data = NULL;

static GListStore *model = NULL;
static GtkFilter *filter = NULL;

static void on_task_list_entry_activated_cb(ErrandsTaskList *self);
static void on_task_list_entry_text_changed_cb(ErrandsTaskList *self);
static void on_task_list_search_cb(ErrandsTaskList *self, GtkSearchEntry *entry);
static void on_motion_cb(GtkEventControllerMotion *ctrl, gdouble x, gdouble y, ErrandsTaskList *self);

static void on_focus_entry_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);
static void on_entry_task_menu_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskList *self);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTaskList, errands_task_list, ADW_TYPE_BIN)

static void errands_task_list_dispose(GObject *gobject) {
  if (entry_task) g_object_run_dispose(G_OBJECT(entry_task));
  errands_task_data_free(entry_task_data);
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
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, task_menu);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, motion_ctrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, scrl);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskList, list_view);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_sort_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_entry_text_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_task_list_search_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_motion_cb);
}

static gboolean filter_func(GtkTreeListRow *row, ErrandsTaskList *self) {
  ErrandsTaskItem *item = ERRANDS_TASK_ITEM(gtk_tree_list_row_get_item(row));
  bool result = false;

  switch (self->page) {
  case ERRANDS_TASK_LIST_PAGE_TODAY: break;
  case ERRANDS_TASK_LIST_PAGE_PINNED: break;
  case ERRANDS_TASK_LIST_PAGE_ALL: result = true; break;
  case ERRANDS_TASK_LIST_PAGE_TASK_LIST: result = errands_task_item_get_task_data(item)->list == self->data; break;
  }

  return result;
}

static GListModel *task_children_func(gpointer item, gpointer user_data) {
  ErrandsTaskItem *task_item = ERRANDS_TASK_ITEM(item);
  GListModel *model = errands_task_item_get_children_model(task_item);
  if (!model) return NULL;
  return g_object_ref(model);
}

static void on_setup_item(GtkSignalListItemFactory *self, GtkListItem *list_item) {
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_tree_expander_new());

  ErrandsTask *task = errands_task_new();
  gtk_tree_expander_set_child(expander, GTK_WIDGET(task));

  gtk_list_item_set_child(list_item, GTK_WIDGET(expander));
}

static void on_bind_item(GtkListItemFactory *factory, GtkListItem *list_item) {
  GtkTreeListRow *row = gtk_list_item_get_item(list_item);
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(list_item));

  gtk_tree_expander_set_list_row(expander, row);

  ErrandsTask *task = ERRANDS_TASK(gtk_tree_expander_get_child(expander));
  ErrandsTaskItem *item = gtk_tree_list_row_get_item(row);

  errands_task_set_data(task, errands_task_item_get_task_data(item));
  task->item = item;
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

  model = g_list_store_new(ERRANDS_TYPE_TASK_ITEM);
  for_range(i, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, i);
    for_range(j, 0, list->children->len) {
      TaskData *data = g_ptr_array_index(list->children, j);
      if (!data->parent) {
        g_autoptr(ErrandsTaskItem) item = errands_task_item_new(data);
        g_list_store_append(model, item);
      }
    }
  }
  GtkTreeListModel *tree_model =
      gtk_tree_list_model_new(G_LIST_MODEL(model), FALSE, FALSE, task_children_func, NULL, NULL);

  filter = GTK_FILTER(gtk_custom_filter_new((GtkCustomFilterFunc)filter_func, self, NULL));
  GtkFilterListModel *filter_model = gtk_filter_list_model_new(G_LIST_MODEL(tree_model), filter);

  GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
  g_signal_connect(factory, "setup", G_CALLBACK(on_setup_item), self);
  g_signal_connect(factory, "bind", G_CALLBACK(on_bind_item), self);

  GtkSelectionModel *selection_model = GTK_SELECTION_MODEL(gtk_no_selection_new(G_LIST_MODEL(filter_model)));

  gtk_list_view_set_model(GTK_LIST_VIEW(self->list_view), selection_model);
  gtk_list_view_set_factory(GTK_LIST_VIEW(self->list_view), factory);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(self->scrl), self->list_view);
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

// ---------- TASKS RECYCLER ---------- //

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

static void __filter_cb(gpointer change) { gtk_filter_changed(filter, GPOINTER_TO_INT(change)); }

void errands_task_list_show_today_tasks(ErrandsTaskList *self) {
  LOG("Task List: Show today tasks");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_TODAY;
  gtk_widget_set_visible(self->entry_box, false);
}

void errands_task_list_show_all_tasks(ErrandsTaskList *self) {
  LOG("Task List: Show all tasks");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_ALL;
  gtk_widget_set_visible(self->entry_box, false);
  g_idle_add_once((GSourceOnceFunc)__filter_cb, GINT_TO_POINTER(GTK_FILTER_CHANGE_LESS_STRICT));
}

void errands_task_list_show_task_list(ErrandsTaskList *self, ListData *data) {
  self->data = data;
  self->page = ERRANDS_TASK_LIST_PAGE_TASK_LIST;
  gtk_widget_set_visible(self->entry_box, true);
  g_idle_add_once((GSourceOnceFunc)__filter_cb, GINT_TO_POINTER(GTK_FILTER_CHANGE_DIFFERENT));
}

void errands_task_list_show_pinned(ErrandsTaskList *self) {
  LOG("Task List: Show pinned");
  self->data = NULL;
  self->page = ERRANDS_TASK_LIST_PAGE_PINNED;
  gtk_widget_set_visible(self->entry_box, false);
}

void errands_task_list_reload(ErrandsTaskList *self, bool save_scroll_pos) {}

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
  g_list_store_insert(model, 0, errands_task_item_new(data));
  // Reload entry task
  entry_task->data->ical = icalcomponent_new(ICAL_VTODO_COMPONENT);
  // Reset text
  g_object_set(self->entry, "text", "", NULL);
  // Update UI
  errands_sidebar_task_list_row_update(errands_sidebar_task_list_row_get(data->list));
  errands_sidebar_update_filter_rows();
  LOG("Add task '%s' to task list '%s'", errands_data_get_uid(data->ical), list_uid);
  // errands_list_data_sort(self->data);
  // errands_task_list_reload(self, false);
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
