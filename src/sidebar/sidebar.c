#include "sidebar.h"
#include "../components.h"
#include "../data/data.h"
#include "../settings.h"
#include "../state.h"
#include "../task-list/task-list.h"
#include "../utils.h"
#include "../window.h"
#include "glib.h"

#include <glib/gi18n.h>

// --- DECLARATIONS --- //

static void on_errands_sidebar_filter_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data);
static void on_import_action(GSimpleAction *action, GVariant *param, ErrandsSidebar *sb);

// --- IMPLEMENTATIONS --- //

G_DEFINE_TYPE(ErrandsSidebar, errands_sidebar, ADW_TYPE_BIN)

static void errands_sidebar_class_init(ErrandsSidebarClass *class) {}

static void errands_sidebar_init(ErrandsSidebar *self) {
  LOG("Sidebar: Create");
  g_autoptr(GSimpleActionGroup) ag = errands_action_group_new(1, "import", on_import_action, self);
  gtk_widget_insert_action_group(GTK_WIDGET(self), "sidebar", G_ACTION_GROUP(ag));

  // Toolbar View
  GtkWidget *tb = adw_toolbar_view_new();
  adw_bin_set_child(ADW_BIN(self), tb);

  // Headerbar
  GtkWidget *hb = adw_header_bar_new();
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), adw_window_title_new("Errands", ""));
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);

  // Add list button
  self->add_btn = adw_split_button_new();
  g_autoptr(GMenu) menu = errands_menu_new(1, _("Import \".ics\""), "sidebar.import");
  g_object_set(self->add_btn, "tooltip-text", _("Add Task List (Ctrl+Shift+L)"), "icon-name", "errands-add-symbolic",
               "menu-model", menu, NULL);
  g_signal_connect(self->add_btn, "clicked", G_CALLBACK(errands_new_list_dialog_show), NULL);
  errands_add_shortcut(self->add_btn, "<Control><Shift>L", "activate");
  adw_header_bar_pack_start(ADW_HEADER_BAR(hb), self->add_btn);

  // Menu button
  GtkWidget *menu_btn = gtk_menu_button_new();
  g_object_set(menu_btn, "icon-name", "open-menu-symbolic", NULL);
  adw_header_bar_pack_end(ADW_HEADER_BAR(hb), menu_btn);

  // Filter rows
  self->filters_box = gtk_list_box_new();
  gtk_widget_add_css_class(self->filters_box, "navigation-sidebar");
  g_signal_connect(self->filters_box, "row-activated", G_CALLBACK(on_errands_sidebar_filter_row_activated), NULL);

  // All tasks row
  self->all_row = errands_sidebar_all_row_new();
  errands_sidebar_all_row_update_counter(self->all_row);
  gtk_list_box_append(GTK_LIST_BOX(self->filters_box), GTK_WIDGET(self->all_row));

  // // Today row
  // GtkWidget *today_row = errands_sidebar_row_new(
  //     "errands_today_page", "errands-today-symbolic", "Today", NULL);
  // gtk_list_box_append(GTK_LIST_BOX(state.filters_list_box), today_row);

  // // Trash row
  // GtkWidget *trash_row = errands_sidebar_row_new(
  //     "errands_trash_page", "errands-trash-symbolic", "Trash", NULL);
  // gtk_list_box_append(GTK_LIST_BOX(state.filters_list_box), trash_row);

  // Task Lists rows
  self->task_lists_box = gtk_list_box_new();
  gtk_widget_add_css_class(self->task_lists_box, "navigation-sidebar");
  g_signal_connect(self->task_lists_box, "row-activated", G_CALLBACK(on_errands_sidebar_task_list_row_activate), NULL);

  // Add rows
  size_t count = 0;
  GPtrArray *lists = g_hash_table_get_values_as_ptr_array(ldata);
  for (size_t i = 0; i < lists->len; i++) {
    ListData *ld = lists->pdata[i];
    if (!errands_data_get_bool(ld, DATA_PROP_DELETED)) {
      ErrandsSidebarTaskListRow *row = errands_sidebar_task_list_row_new(ld);
      gtk_list_box_append(GTK_LIST_BOX(self->task_lists_box), GTK_WIDGET(row));
      count++;
    }
  }
  g_object_set(state.main_window->no_lists_page, "visible", count == 0, NULL);

  // Sidebar content box
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(box), self->filters_box);
  gtk_box_append(GTK_BOX(box), g_object_new(GTK_TYPE_SEPARATOR, "margin-start", 8, "margin-end", 8, NULL));
  gtk_box_append(GTK_BOX(box), self->task_lists_box);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl), box);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), scrl);

  // Select last opened page
  g_signal_connect(state.main_window, "realize", G_CALLBACK(errands_sidebar_select_last_opened_page), NULL);
}

ErrandsSidebar *errands_sidebar_new() { return g_object_new(ERRANDS_TYPE_SIDEBAR, NULL); }

ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(ErrandsSidebar *sb, ListData *data) {
  LOG("Sidebar: Add task list '%s'", errands_data_get_str(data, DATA_PROP_LIST_UID));
  ErrandsSidebarTaskListRow *row = errands_sidebar_task_list_row_new(data);
  gtk_list_box_append(GTK_LIST_BOX(sb->task_lists_box), GTK_WIDGET(row));
  return row;
}

void errands_sidebar_select_last_opened_page() {
  GPtrArray *rows = get_children(state.sidebar->task_lists_box);
  const char *last_uid = errands_settings_get("last_list_uid", SETTING_TYPE_STRING).s;
  LOG("Sidebar: Selecting last opened list: '%s'", last_uid);
  for (size_t i = 0; i < rows->len; i++) {
    ErrandsSidebarTaskListRow *row = rows->pdata[i];
    if (g_str_equal(last_uid, errands_data_get_str(row->data, DATA_PROP_LIST_UID)))
      g_signal_emit_by_name(row, "activate", NULL);
  }
}

// --- SIGNAL HANDLERS --- //

static void on_errands_sidebar_filter_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.sidebar->task_lists_box));
  if (GTK_WIDGET(row) == GTK_WIDGET(state.sidebar->all_row)) {
    LOG("Sidebar: Switch to all tasks page");
    adw_view_stack_set_visible_child_name(ADW_VIEW_STACK(state.main_window->stack), "errands_task_list_page");
    state.task_list->data = NULL;
    errands_task_list_update_title();
    errands_task_list_filter_by_uid("");
    gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), false);
  }
}

static void __on_open_finish(GObject *obj, GAsyncResult *res, ErrandsSidebar *sb) {
  g_autoptr(GFile) file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!file) return;
  g_autofree gchar *path = g_file_get_path(file);
  char *ical = read_file_to_string(path);
  if (ical) {
    g_autofree gchar *basename = g_file_get_basename(file);
    *(strrchr(basename, '.')) = '\0';
    ListData *data = list_data_new_from_ical(ical, basename, g_hash_table_size(ldata));
    free(ical);
    // Check if uid exists
    bool exists = g_hash_table_contains(ldata, errands_data_get_str(data, DATA_PROP_LIST_UID));
    if (!exists) {
      LOG("Sidebar: List already exists");
      errands_window_add_toast(state.main_window, _("List already exists"));
      errands_data_free(data);
      return;
    }
    g_hash_table_insert(ldata, strdup(errands_data_get_str(data, DATA_PROP_LIST_UID)), data);
    errands_sidebar_add_task_list(sb, data);
    GPtrArray *tasks = list_data_get_tasks(data);
    for (size_t i = 0; i < tasks->len; i++) {
      TaskData *td = tasks->pdata[i];
      g_hash_table_insert(tdata, strdup(errands_data_get_str(data, DATA_PROP_UID)), td);
      errands_task_list_add(td);
    }
    errands_data_write_list(data);
    errands_task_list_filter_by_uid(basename);
  }
  // TODO: sync
}

static void on_import_action(GSimpleAction *action, GVariant *param, ErrandsSidebar *sb) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  gtk_file_dialog_open(dialog, GTK_WINDOW(state.main_window), NULL, (GAsyncReadyCallback)__on_open_finish, sb);
}
