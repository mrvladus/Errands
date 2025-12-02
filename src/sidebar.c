#include "sidebar.h"
#include "about-dialog.h"
#include "data.h"
#include "settings-dialog.h"
#include "settings.h"
#include "state.h"
#include "sync.h"
#include "task-list.h"
#include "utils.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <stddef.h>

static void on_errands_sidebar_filter_row_activated(GtkListBox *box, GtkListBoxRow *row, ErrandsSidebar *self);
static void on_import_action_cb(GSimpleAction *action, GVariant *param, ErrandsSidebar *self);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsSidebar, errands_sidebar, ADW_TYPE_BIN)

static void errands_sidebar_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR);
  G_OBJECT_CLASS(errands_sidebar_parent_class)->dispose(gobject);
}

static void errands_sidebar_class_init(ErrandsSidebarClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_sidebar_dispose;

  g_type_ensure(ERRANDS_TYPE_SIDEBAR_ALL_ROW);
  g_type_ensure(ERRANDS_TYPE_SIDEBAR_TODAY_ROW);
  g_type_ensure(ERRANDS_TYPE_SIDEBAR_PINNED_ROW);
  g_type_ensure(ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW);

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/io/github/mrvladus/Errands/ui/sidebar.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebar, filters_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebar, all_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebar, today_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebar, pinned_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebar, task_lists_box);

  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_errands_sidebar_filter_row_activated);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_errands_sidebar_task_list_row_activate);
}

static void errands_sidebar_init(ErrandsSidebar *self) {
  LOG("Sidebar: Create");
  gtk_widget_init_template(GTK_WIDGET(self));
  errands_add_actions(GTK_WIDGET(self), "sidebar", "import", on_import_action_cb, self, "new_list",
                      errands_sidebar_new_list_dialog_show, NULL, "preferences", errands_settings_dialog_show, NULL,
                      "about", errands_about_dialog_show, NULL, NULL);
}

ErrandsSidebar *errands_sidebar_new() { return g_object_new(ERRANDS_TYPE_SIDEBAR, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_sidebar_load_lists(ErrandsSidebar *self) {
  LOG("Sidebar: Create Task List Rows");
  // Add rows
  for (size_t i = 0; i < errands_data_lists->len; i++) {
    ListData *ld = errands_data_lists->pdata[i];
    if (!errands_data_get_bool(ld->data, DATA_PROP_DELETED)) {
      ErrandsSidebarTaskListRow *row = errands_sidebar_task_list_row_new(ld);
      gtk_list_box_append(GTK_LIST_BOX(self->task_lists_box), GTK_WIDGET(row));
    }
  }
  errands_sidebar_update_filter_rows(self);
  errands_window_update(state.main_window);
  // Select last opened page
  g_signal_connect(state.main_window, "realize", G_CALLBACK(errands_sidebar_select_last_opened_page), NULL);
  LOG("Sidebar: Created %d Task List Rows", errands_data_lists->len);
}

ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(ErrandsSidebar *sb, ListData *data) {
  LOG("Sidebar: Add task list '%s'", errands_data_get_str(data->data, DATA_PROP_LIST_UID));
  ErrandsSidebarTaskListRow *row = errands_sidebar_task_list_row_new(data);
  gtk_list_box_append(GTK_LIST_BOX(sb->task_lists_box), GTK_WIDGET(row));
  return row;
}

void errands_sidebar_select_last_opened_page() {
  g_autoptr(GPtrArray) rows = get_children(state.main_window->sidebar->task_lists_box);
  const char *last_uid = errands_settings_get(SETTING_LAST_LIST_UID).s;
  LOG("Sidebar: Selecting last opened list: '%s'", last_uid);
  for_range(i, 0, rows->len) {
    ErrandsSidebarTaskListRow *row = g_ptr_array_index(rows, i);
    if (STR_EQUAL(last_uid, errands_data_get_str(row->data->data, DATA_PROP_LIST_UID)))
      g_signal_emit_by_name(row, "activate", NULL);
  }
}

void errands_sidebar_update_filter_rows(ErrandsSidebar *self) {
  size_t total = 0, completed = 0, today = 0, today_completed = 0, pinned = 0;
  for_range(l, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, l);
    g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(list);
    for_range(t, 0, tasks->len) {
      TaskData *task = g_ptr_array_index(errands_data_lists, t);
      CONTINUE_IF(errands_data_get_bool(task->data, DATA_PROP_DELETED));
      bool is_completed = errands_task_data_is_completed(task);
      if (is_completed) completed++;
      if (errands_task_data_is_due(task)) {
        today++;
        if (is_completed) today_completed++;
      }
      if (errands_data_get_bool(task->data, DATA_PROP_PINNED)) pinned++;
      total++;
    }
  }
  const char *all_label = total - completed > 0 ? tmp_str_printf("%zu", total - completed) : "";
  gtk_label_set_label(self->all_row->counter, all_label);
  const char *today_label = today - today_completed > 0 ? tmp_str_printf("%zu", today - today_completed) : "";
  gtk_label_set_label(self->today_row->counter, today_label);
  const char *pinned_label = pinned > 0 ? tmp_str_printf("%zu", pinned) : "";
  gtk_label_set_label(self->pinned_row->counter, pinned_label);
}

// --- SIGNAL HANDLERS --- //

static void on_errands_sidebar_filter_row_activated(GtkListBox *box, GtkListBoxRow *row, ErrandsSidebar *self) {
  gtk_list_box_unselect_all(GTK_LIST_BOX(self->task_lists_box));
  ErrandsTaskList *task_list = state.main_window->task_list;
  if (GTK_WIDGET(row) == GTK_WIDGET(self->all_row)) errands_task_list_show_all_tasks(task_list);
  else if (GTK_WIDGET(row) == GTK_WIDGET(self->today_row)) errands_task_list_show_today_tasks(task_list);
  else if (GTK_WIDGET(row) == GTK_WIDGET(self->pinned_row)) errands_task_list_show_pinned(task_list);
}

static void __on_open_finish(GObject *obj, GAsyncResult *res, ErrandsSidebar *self) {
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
    if (STR_EQUAL(uid, errands_data_get_str(data->data, DATA_PROP_LIST_UID))) {
      errands_window_add_toast(state.main_window, _("List already exists"));
      return;
    }
  }
  ListData *data = errands_list_data_new_from_ical(ical, uid, uid, NULL);
  errands_data_write_list(data);
  g_ptr_array_add(errands_data_lists, data);
  ErrandsSidebarTaskListRow *row = errands_sidebar_add_task_list(self, data);
  g_signal_emit_by_name(row, "activate", NULL);
  errands_sync_schedule_list(data);
}

static void on_import_action_cb(GSimpleAction *action, GVariant *param, ErrandsSidebar *self) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  g_autoptr(GtkFileFilter) filter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(filter, "*.ics");
  gtk_file_dialog_set_default_filter(dialog, filter);
  gtk_file_dialog_open(dialog, GTK_WINDOW(state.main_window), NULL, (GAsyncReadyCallback)__on_open_finish, self);
}
