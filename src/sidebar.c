#include "sidebar.h"
#include "about-dialog.h"
#include "adwaita.h"
#include "data.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "settings-dialog.h"
#include "settings.h"
#include "state.h"
#include "sync.h"
#include "task-list-item.h"
#include "task-list.h"
#include "utils.h"

#include <glib/gi18n.h>

static void on_import_action_cb(GSimpleAction *action, GVariant *param);
static void on_sidebar_activated_cb(AdwSidebar *self, guint index, gpointer user_data);

static ErrandsSidebar *self = NULL;

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsSidebar, errands_sidebar, ADW_TYPE_BIN)

static void errands_sidebar_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR);
  G_OBJECT_CLASS(errands_sidebar_parent_class)->dispose(gobject);
  // TODO: unref dialogs
}

static void errands_sidebar_class_init(ErrandsSidebarClass *klass) {
  G_OBJECT_CLASS(klass)->dispose = errands_sidebar_dispose;

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), RESOURCE_PATH "/ui/sidebar.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, sync_indicator);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, sidebar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, all_counter);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, today_counter);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, task_lists_section);

  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_sidebar_activated_cb);
}

static AdwSidebarItem *create_sidebar_task_list_item_create_func(ErrandsTaskListItem *item, gpointer data) {
  AdwSidebarItem *self = adw_sidebar_item_new(item->title);
  g_object_set_data(G_OBJECT(self), "item", item);
  g_object_bind_property(item, "title", self, "title", G_BINDING_SYNC_CREATE);
  GtkWidget *counter = gtk_label_new(NULL);
  g_object_bind_property(item, "count-string", counter, "label", G_BINDING_SYNC_CREATE);
  gtk_widget_add_css_class(counter, "dim-label");
  gtk_widget_add_css_class(counter, "caption");
  GtkColorDialog *dialog = gtk_color_dialog_new();
  GtkWidget *color = gtk_color_dialog_button_new(dialog);
  g_object_bind_property(item, "color", color, "rgba", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append(GTK_BOX(box), counter);
  gtk_box_append(GTK_BOX(box), color);
  adw_sidebar_item_set_suffix(self, box);
  errands_sidebar_task_list_update_counter(item->uid);
  return self;
}

void errands_sidebar_task_list_update_counter(const char *uid) {
  errands_task_list_item_update_counter(errands_sidebar_find_list(uid));
}

static void errands_sidebar_init(ErrandsSidebar *sidebar) {
  self = sidebar;
  gtk_widget_init_template(GTK_WIDGET(self));
  GSimpleActionGroup *ag = errands_add_action_group(self, "sidebar");
  errands_add_action(ag, "import", on_import_action_cb, self, NULL);
  errands_add_action(ag, "new_list", errands_new_list_dialog_show, self, NULL);
  errands_add_action(ag, "preferences", errands_settings_dialog_show, self, NULL);
  errands_add_action(ag, "about", errands_about_dialog_show, self, NULL);
  errands_add_action(ag, "sync", errands_sync, self, NULL);

  self->task_lists_model = g_list_store_new(ERRANDS_TYPE_TASK_LIST_ITEM);
  adw_sidebar_section_bind_model(self->task_lists_section, G_LIST_MODEL(self->task_lists_model),
                                 (AdwSidebarSectionCreateItemFunc)create_sidebar_task_list_item_create_func, NULL,
                                 NULL);
}

ErrandsSidebar *errands_sidebar_new() { return g_object_new(ERRANDS_TYPE_SIDEBAR, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

static gint __sort_func(gconstpointer a, gconstpointer b) {
  ListData *ld_a = (ListData *)a;
  ListData *ld_b = (ListData *)b;
  g_autofree gchar *folded1 = g_utf8_casefold(errands_data_get_list_name(ld_a->ical), -1);
  g_autofree gchar *folded2 = g_utf8_casefold(errands_data_get_list_name(ld_b->ical), -1);
  return g_utf8_collate(folded1, folded2);
}

void errands_sidebar_load_lists(void) {
  g_ptr_array_sort_values(errands_data_lists, __sort_func);
  // Add rows
  for (size_t i = 0; i < errands_data_lists->len; i++) {
    ListData *ld = errands_data_lists->pdata[i];
    if (!errands_data_get_deleted(ld->ical)) {
      ErrandsTaskListItem *list_item = errands_task_list_item_new(ld);
      g_list_store_append(self->task_lists_model, list_item);
    }
  }
  errands_sidebar_update_filter_rows();
  errands_sidebar_select_last_opened_page();
}

ErrandsTaskListItem *errands_sidebar_find_list(const char *uid) {
  if (!uid) return NULL;
  GListModel *model = G_LIST_MODEL(self->task_lists_model);
  for (size_t i = 0; i < g_list_model_get_n_items(model); i++) {
    ErrandsTaskListItem *item = g_list_model_get_item(model, i);
    if (g_str_equal(uid, item->uid)) return item;
  }
  return NULL;
}

void errands_sidebar_select_last_opened_page(void) {
  const char *last_uid = errands_settings_get(SETTING_LAST_LIST_UID).s;
  int idx = -1;
  GListModel *model = G_LIST_MODEL(self->task_lists_model);
  for (size_t i = 0; i < g_list_model_get_n_items(model); i++) {
    ErrandsTaskListItem *item = g_list_model_get_item(model, i);
    if (item->uid && g_str_equal(last_uid, item->uid)) {
      idx = i;
      break;
    }
  }
  idx = idx == -1 ? 0 : idx + 2;
  adw_sidebar_set_selected(ADW_SIDEBAR(self->sidebar), idx);
  g_signal_emit_by_name(self->sidebar, "activated", idx, NULL);
}

void errands_sidebar_update_filter_rows(void) {
  size_t total = 0, completed = 0, today = 0, today_completed = 0, n_lists = 0;
  for_range(l, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, l);
    CONTINUE_IF(errands_data_get_deleted(list->ical));
    n_lists++;
    g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(list);
    for_range(t, 0, tasks->len) {
      icalcomponent *ical = g_ptr_array_index(tasks, t);
      CONTINUE_IF(errands_data_get_deleted(ical) || errands_data_get_cancelled(ical));
      bool is_completed = !icaltime_is_null_date(errands_data_get_completed(ical));
      bool is_due = errands_data_is_due(ical);
      if (is_completed) completed++;
      if (is_due) {
        today++;
        if (is_completed) today_completed++;
      }
      total++;
    }
  }
  const char *all_label = total - completed > 0 ? tmp_str_printf("%zu", total - completed) : "";
  const char *today_label = today - today_completed > 0 ? tmp_str_printf("%zu", today - today_completed) : "";
  gtk_label_set_label(self->all_counter, all_label);
  gtk_label_set_label(self->today_counter, today_label);
  gtk_widget_set_visible(self->sidebar, n_lists > 0);
}

void errands_sidebar_toggle_sync_indicator(bool on) { gtk_widget_set_visible(self->sync_indicator, on); }

// --- SIGNAL HANDLERS --- //

static void on_sidebar_activated_cb(AdwSidebar *self, guint index, gpointer user_data) {
  ErrandsTaskList *task_list = state.main_window->task_list;
  if (index == 0) errands_task_list_show_all_tasks(task_list);
  else if (index == 1) errands_task_list_show_today_tasks(task_list);
  else {
    AdwSidebarItem *item = adw_sidebar_get_item(self, index);
    g_assert(item);
    ErrandsTaskListItem *tl_item = g_object_get_data(G_OBJECT(item), "item");
    errands_settings_set(SETTING_LAST_LIST_UID, (void *)tl_item->uid);
    errands_task_list_show_task_list(task_list, tl_item->data);
  }
  adw_navigation_split_view_set_show_content(state.main_window->split_view, true);
}

static void __on_open_finish(GObject *obj, GAsyncResult *res) {
  g_autoptr(GFile) file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!file) return;
  g_autofree gchar *path = g_file_get_path(file);
  autofree char *ical = read_file_to_string(path);
  if (!ical) return;
  char *uid = (char *)path_base_name(path);
  *(strrchr(uid, '.')) = '\0';
  // Check if uid exists
  for_range(i, 0, errands_data_lists->len) {
    ListData *data = g_ptr_array_index(errands_data_lists, i);
    if (g_str_equal(uid, errands_data_get_uid(data->ical))) {
      errands_window_add_toast(_("List already exists"), 2);
      return;
    }
  }
  icalcomponent *ical_comp = icalparser_parse_string(ical);
  if (!ical_comp) return;
  ListData *data = errands_list_data_load_from_ical(ical_comp, uid, NULL, NULL);
  errands_list_data_save(data);
  g_ptr_array_add(errands_data_lists, data);
  ErrandsTaskListItem *list_item = errands_task_list_item_new(data);
  g_list_store_append(self->task_lists_model, list_item);
  errands_sync_create_list(data);
  errands_sidebar_update_filter_rows();
  errands_settings_set(SETTING_LAST_LIST_UID, (void *)uid);
  errands_sidebar_select_last_opened_page();
}

static void on_import_action_cb(GSimpleAction *action, GVariant *param) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  g_autoptr(GtkFileFilter) filter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(filter, "*.ics");
  gtk_file_dialog_set_default_filter(dialog, filter);
  gtk_file_dialog_open(dialog, GTK_WINDOW(state.main_window), NULL, (GAsyncReadyCallback)__on_open_finish, self);
}

// ---------- CALLBACKS ---------- /

// --- DND --- //

// static guint on_drop_motion_ctrl_enter_timeout_cb(ErrandsTaskListRow *self) {
//   if (!gtk_drop_controller_motion_contains_pointer(self->drop_motion_ctrl)) return G_SOURCE_REMOVE;
//   if (errands_sidebar_row_is_selected(self)) return G_SOURCE_REMOVE;
//   gtk_widget_activate(GTK_WIDGET(self));

//   return G_SOURCE_REMOVE;
// }

// static void on_drop_motion_ctrl_enter_cb(ErrandsTaskListRow *self) {
//   g_timeout_add_once(500, (GSourceOnceFunc)on_drop_motion_ctrl_enter_timeout_cb, self);
// }

// static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTaskListRow *self)
// {
//   ErrandsTaskItem *drop_item = g_value_get_object(value);
//   TaskData *drop_data = errands_task_item_get_data(drop_item);
//   ListData *list_data = self->item->data;
//   ListData *old_list_data = drop_data->list;

//   bool changing_list = old_list_data != list_data;

//   ErrandsTaskItem *drop_item_parent = errands_task_item_get_parent(drop_item);

//   // Don't move toplevel task in the same list
//   if (!changing_list && !drop_item_parent) return false;

//   // Get old parent task
//   ErrandsTask *old_parent_task = NULL;
//   if (drop_data->parent && drop_item_parent) g_object_get(drop_item_parent, "task-widget", &old_parent_task, NULL);

//   // Move data
//   bool moved = errands_task_data_move_to_list(drop_data, list_data, NULL);
//   if (!moved) return false;
//   errands_list_data_save(list_data);
//   if (changing_list) errands_list_data_save(old_list_data);

//   // Update task model if necessary
//   if (drop_item_parent) {
//     // Add the item to the current task model
//     GListStore *parent_model = G_LIST_STORE(errands_task_item_get_children_model(drop_item_parent));
//     guint idx;
//     if (g_list_store_find(parent_model, drop_item, &idx)) g_list_store_remove(parent_model, idx);
//     g_object_notify(G_OBJECT(drop_item_parent), "children-model-is-empty");
//     errands_task_item_set_parent(drop_item, NULL);
//   }

//   // Update filter
//   errands_task_list_filter_toplevel(state.main_window->task_list, GTK_FILTER_CHANGE_DIFFERENT);
//   if (old_parent_task) errands_task_update_progress(old_parent_task);

//   // Update the UI after the operation
//   // if (changing_list) errands_task_list_row_update(errands_task_list_row_get(old_list_data));
//   // errands_task_list_row_update(self);

//   return true;
// }
