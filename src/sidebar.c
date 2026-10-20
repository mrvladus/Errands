// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "sidebar.h"
#include "about-dialog.h"
#include "config.h"
#include "data.h"
#include "settings-dialog.h"
#include "settings.h"
#include "state.h"
#include "sync.h"
#include "task-item.h"
#include "task-list-item.h"
#include "task-list.h"
#include "utils.h"

#include "adwaita.h"
#include "gdk/gdk.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include <glib/gi18n.h>

static gint sort_func(gconstpointer a, gconstpointer b, gpointer data);

static void on_import_action_cb(GSimpleAction *action, GVariant *param);
static void on_sync_action_cb();

static void on_sidebar_activated_cb(AdwSidebar *self, guint index, gpointer user_data);
static gboolean on_drop_cb(AdwSidebar *self, guint index, const GValue *value, GdkDragAction preferred_action,
                           gpointer user_data);
static void on_entry_changed_cb(ErrandsSidebar *self, GtkEntry *entry);
static void on_entry_activated_cb(ErrandsSidebar *self, GtkEntry *entry);
static void on_create_btn_clicked_cb(ErrandsSidebar *self, GtkButton *btn);

static ErrandsSidebar *self = NULL;

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsSidebar, errands_sidebar, ADW_TYPE_BIN)

static void finalize(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR);
  G_OBJECT_CLASS(errands_sidebar_parent_class)->finalize(gobject);
}

static void errands_sidebar_class_init(ErrandsSidebarClass *klass) {
  G_OBJECT_CLASS(klass)->finalize = finalize;

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), RESOURCE_PATH "/ui/sidebar.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, popover);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, create_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, sidebar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, all_counter);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, today_counter);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebar, task_lists_section);

  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_sidebar_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_entry_changed_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_create_btn_clicked_cb);
}

static gboolean counter_binding_transform_func(GBinding *binding, const GValue *from_value, GValue *to_value,
                                               gpointer user_data) {
  gint count = g_value_get_int(from_value);
  g_value_set_string(to_value, count <= 0 ? "" : g_strdup_printf("%d", count));
  return true;
}

static AdwSidebarItem *create_sidebar_task_list_item_create_func(ErrandsTaskListItem *item, gpointer data) {
  AdwSidebarItem *self = adw_sidebar_item_new(item->title);
  g_object_set_data(G_OBJECT(self), "item", item);
  g_object_bind_property(item, "title", self, "title", G_BINDING_SYNC_CREATE);
  GtkWidget *counter = gtk_label_new(NULL);
  g_object_bind_property_full(item, "count", counter, "label", G_BINDING_SYNC_CREATE, counter_binding_transform_func, NULL, NULL,
                              NULL);
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

static void errands_sidebar_init(ErrandsSidebar *sidebar) {
  self = sidebar;
  gtk_widget_init_template(GTK_WIDGET(self));

  GSimpleActionGroup *ag = errands_add_action_group(self, "sidebar");
  errands_add_action(ag, "import", on_import_action_cb, self, NULL);
  errands_add_action(ag, "sync", on_sync_action_cb, NULL, NULL);
  errands_add_action(ag, "preferences", errands_settings_dialog_show, self, NULL);
  errands_add_action(ag, "about", errands_about_dialog_show, self, NULL);

  GType dnd_types[] = {ERRANDS_TYPE_TASK_ITEM};
  adw_sidebar_setup_drop_target(ADW_SIDEBAR(self->sidebar), GDK_ACTION_MOVE | GDK_ACTION_COPY, dnd_types,
                                G_N_ELEMENTS(dnd_types));
  g_signal_connect(self->sidebar, "drop", G_CALLBACK(on_drop_cb), NULL);

  adw_sidebar_section_bind_model(self->task_lists_section, G_LIST_MODEL(task_lists_model),
                                 (AdwSidebarSectionCreateItemFunc)create_sidebar_task_list_item_create_func, NULL, NULL);

  g_list_store_sort(task_lists_model, sort_func, NULL);
  errands_sidebar_update_filter_rows();
}

ErrandsSidebar *errands_sidebar_new() { return g_object_new(ERRANDS_TYPE_SIDEBAR, NULL); }

// ---------- PRIVATE ---------- //

static gint sort_func(gconstpointer a, gconstpointer b, gpointer data) {
  ErrandsTaskListItem *item_a = (ErrandsTaskListItem *)a;
  ErrandsTaskListItem *item_b = (ErrandsTaskListItem *)b;
  g_autofree gchar *folded1 = g_utf8_casefold(item_a->title, -1);
  g_autofree gchar *folded2 = g_utf8_casefold(item_b->title, -1);
  return g_utf8_collate(folded1, folded2);
}

// ---------- PUBLIC ---------- //

ErrandsTaskListItem *errands_sidebar_find_list(const char *uid) {
  if (!uid) return NULL;
  GListModel *model = G_LIST_MODEL(task_lists_model);
  for (size_t i = 0; i < g_list_model_get_n_items(model); i++) {
    g_autoptr(ErrandsTaskListItem) item = g_list_model_get_item(model, i);
    if (g_str_equal(uid, item->uid)) return item;
  }
  return NULL;
}

void errands_sidebar_select_last_opened_page(void) {
  const char *last_uid = errands_settings_get_last_list_uid(state.settings);
  int idx = -1;
  GListModel *model = G_LIST_MODEL(task_lists_model);
  for (size_t i = 0; i < g_list_model_get_n_items(model); i++) {
    g_autoptr(ErrandsTaskListItem) item = g_list_model_get_item(model, i);
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
  bool show_completed = errands_settings_get_show_completed(state.settings);
  bool show_cancelled = errands_settings_get_show_cancelled(state.settings);
  GListModel *model = G_LIST_MODEL(task_lists_model);
  int total = 0, completed = 0, today = 0, today_completed = 0, n_lists = g_list_model_get_n_items(model);
  for_range(i, 0, n_lists) {
    g_autoptr(ErrandsTaskListItem) item = g_list_model_get_item(model, i);
    CONTINUE_IF(errands_data_get_deleted(item->ical));
    g_autoptr(GPtrArray) components = vcalendar_to_array(item->ical);
    for (guint j = 0; components && j < components->len; j++) {
      ICalComponent *c = g_ptr_array_index(components, j);
      if (errands_data_get_deleted(c)) continue;
      bool is_completed = errands_data_is_completed(c);
      if (!show_completed && is_completed) continue;
      bool is_cancelled = errands_data_get_cancelled(c);
      if (!show_cancelled && is_cancelled) continue;
      if (is_completed) completed++;
      if (errands_data_is_due(c)) {
        today++;
        if (is_completed) today_completed++;
      }
      total++;
    }
  }
  gtk_label_set_label(self->all_counter, total - completed > 0 ? tmp_str_printf("%d", total - completed) : "");
  gtk_label_set_label(self->today_counter, today - today_completed > 0 ? tmp_str_printf("%d", today - today_completed) : "");
  gtk_widget_set_visible(self->sidebar, n_lists > 0);
}

void errands_sidebar_task_list_update_counter(const char *uid) {
  errands_task_list_item_update_count(errands_sidebar_find_list(uid));
}

void errands_sidebar_delete_list(const char *uid) {
  g_message("Sidebar: Deleting list %s", uid);
  GListModel *model = G_LIST_MODEL(task_lists_model);
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskListItem) item = g_list_model_get_item(model, i);
    if (item->uid && g_str_equal(uid, item->uid)) {
      errands_task_list_item_delete(item);
      break;
    }
  }
  errands_sidebar_update_filter_rows();
}

// --- ACTIONS --- //

static void on_import_action_finish_cb(GObject *obj, GAsyncResult *res) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(obj), res, &error);
  if (!file) {
    if (error && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning("Import: Failed to open file: %s", error->message);
    return;
  }
  g_autofree gchar *path = g_file_get_path(file);
  if (!path) return;
  g_autoptr(ErrandsTaskListItem) item = errands_task_list_item_load_from_ics(path);
  if (!item) return;
  g_list_store_append(task_lists_model, item);
  errands_task_list_item_save(item);
  errands_sync_create_list(item);
  errands_sidebar_update_filter_rows();
  errands_settings_set_last_list_uid(state.settings, item->uid);
  errands_sidebar_select_last_opened_page();
}

static void on_import_action_cb(GSimpleAction *action, GVariant *param) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  g_autoptr(GtkFileFilter) filter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(filter, "*.ics");
  gtk_file_dialog_set_default_filter(dialog, filter);
  gtk_file_dialog_open(dialog, GTK_WINDOW(state.main_window), NULL, (GAsyncReadyCallback)on_import_action_finish_cb, self);
}

static void on_sync_action_cb() { errands_sync(); }

// --- SIGNAL HANDLERS --- //

static void on_sidebar_activated_cb(AdwSidebar *self, guint index, gpointer user_data) {
  ErrandsTaskList *task_list = state.main_window->task_list;
  if (index == 0) errands_task_list_show_all_tasks(task_list);
  else if (index == 1) errands_task_list_show_today_tasks(task_list);
  else {
    AdwSidebarItem *item = adw_sidebar_get_item(self, index);
    g_assert(item);
    ErrandsTaskListItem *tl_item = g_object_get_data(G_OBJECT(item), "item");
    errands_settings_set_last_list_uid(state.settings, tl_item->uid);
    errands_task_list_show_task_list(task_list, tl_item);
  }
  adw_navigation_split_view_set_show_content(state.main_window->split_view, true);
  adw_overlay_split_view_set_show_sidebar(state.main_window->overlay_split_view, false);
}

static void on_entry_changed_cb(ErrandsSidebar *self, GtkEntry *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  gtk_widget_set_sensitive(self->create_btn, !g_str_equal("", text));
}

static void on_entry_activated_cb(ErrandsSidebar *self, GtkEntry *entry) {
  gtk_popover_popdown(GTK_POPOVER(self->popover));
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (g_str_equal(text, "")) return;
  g_autofree gchar *uid = g_uuid_string_random();
  g_autoptr(ErrandsTaskListItem) item = errands_task_list_item_create(uid, text, NULL, false, false);
  g_message("New List Dialog: Create new list: '%s'", item->uid);
  g_list_store_append(task_lists_model, item);
  errands_task_list_item_save(item);
  errands_sync_create_list(item);
  errands_sidebar_update_filter_rows();
  errands_settings_set_last_list_uid(state.settings, item->uid);
  errands_sidebar_select_last_opened_page();
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
}

static void on_create_btn_clicked_cb(ErrandsSidebar *self, GtkButton *btn) {
  g_signal_emit_by_name(self->entry, "activate", self, self->entry, NULL);
}

static gboolean on_drop_cb(AdwSidebar *self, guint index, const GValue *value, GdkDragAction preferred_action,
                           gpointer user_data) {
  if (!G_VALUE_HOLDS(value, ERRANDS_TYPE_TASK_ITEM)) return FALSE;
  ErrandsTaskItem *task = g_value_get_object(value);
  ErrandsTaskListItem *drop_list = errands_task_item_get_list(task);
  g_autoptr(ErrandsTaskListItem) list = g_list_model_get_item(G_LIST_MODEL(task_lists_model), index - 2);
  if (drop_list == list) return FALSE;
  return errands_task_item_move(task, NULL, list);
}
