#include "data.h"
#include "delete-list-dialog.h"
#include "rename-list-dialog.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"
#include "task-item.h"
#include "task-list.h"
#include "task.h"
#include "utils.h"
#include "window.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>

static void on_action_export(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
static void on_action_print(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
static void on_action_rename(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
static void on_action_delete_completed(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
static void on_action_delete_cancelled(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
static void on_action_delete(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);

static void on_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, ListData *data);
static void on_drop_motion_ctrl_enter_cb(ErrandsSidebarTaskListRow *self);
static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y,
                           ErrandsSidebarTaskListRow *row);
// static GdkDragAction on_hover_begin(GtkDropTarget *target, gdouble x, gdouble y,
//                                     ErrandsSidebarTaskListRow *row);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_task_list_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW);
  G_OBJECT_CLASS(errands_sidebar_task_list_row_parent_class)->dispose(gobject);
}

static void errands_sidebar_task_list_row_class_init(ErrandsSidebarTaskListRowClass *klass) {
  G_OBJECT_CLASS(klass)->dispose = errands_sidebar_task_list_row_dispose;

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                              "/io/github/mrvladus/Errands/ui/sidebar-task-list-row.ui");

  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebarTaskListRow, color_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebarTaskListRow, counter);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebarTaskListRow, label);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebarTaskListRow, popover);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsSidebarTaskListRow, drop_motion_ctrl);

  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_drop_motion_ctrl_enter_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_drop_cb);
}

static void errands_sidebar_task_list_row_init(ErrandsSidebarTaskListRow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
  // Actions
  GSimpleActionGroup *ag = errands_add_action_group(self, "task-list-row");
  errands_add_action(ag, "rename", on_action_rename, self, NULL);
  errands_add_action(ag, "print", on_action_print, self, NULL);
  errands_add_action(ag, "export", on_action_export, self, NULL);
  errands_add_action(ag, "delete-completed", on_action_delete_completed, self, NULL);
  errands_add_action(ag, "delete-cancelled", on_action_delete_cancelled, self, NULL);
  errands_add_action(ag, "delete", on_action_delete, self, NULL);
}

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_new(ListData *data) {
  g_assert(data);
  LOG_NO_LN("Task List Row '%s': Create ... ", data->uid);
  ErrandsSidebarTaskListRow *row = g_object_new(ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW, NULL);
  row->data = data;
  // Set color
  GdkRGBA color;
  gdk_rgba_parse(&color, errands_data_get_color(data->ical, true));
  gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(row->color_btn), &color);
  g_signal_connect(row->color_btn, "notify::rgba", G_CALLBACK(on_color_changed), data);
  // Update
  errands_sidebar_task_list_row_update(row);
  LOG_NO_PREFIX("Success");
  return row;
}

// ---------- PUBLIC FUNCTIONS ---------- //

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(ListData *data) {
  g_autoptr(GPtrArray) children = get_children(state.main_window->sidebar->task_lists_box);
  for_range(i, 0, children->len) {
    ListData *child_data = ((ErrandsSidebarTaskListRow *)g_ptr_array_index(children, i))->data;
    if (child_data == data) return children->pdata[i];
  }
  return NULL;
}

void errands_sidebar_task_list_row_update(ErrandsSidebarTaskListRow *self) {
  if (!self) return;
  size_t total = 0, completed = 0;
  g_autoptr(GPtrArray) tasks = errands_list_data_get_all_tasks_as_icalcomponents(self->data);
  for_range(i, 0, tasks->len) {
    icalcomponent *ical = g_ptr_array_index(tasks, i);
    CONTINUE_IF(errands_data_get_deleted(ical));
    if (errands_data_is_completed(ical)) completed++;
    total++;
  }
  size_t uncompleted = total - completed;
  gtk_label_set_label(GTK_LABEL(self->counter), uncompleted > 0 ? tmp_str_printf("%zu", uncompleted) : "");
  gtk_label_set_label(GTK_LABEL(self->label), errands_data_get_list_name(self->data->ical));
}

// ---------- CALLBACKS ---------- //

void on_errands_sidebar_task_list_row_activate(GtkListBox *box, ErrandsSidebarTaskListRow *row, gpointer user_data) {
  ErrandsTaskList *task_list = state.main_window->task_list;
  // Unselect filter rows
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.main_window->sidebar->filters_box));
  // Set setting
  LOG("Switch to list '%s'", row->data->uid);
  errands_settings_set(SETTING_LAST_LIST_UID, (void *)row->data->uid);
  errands_task_list_show_task_list(task_list, row->data);
  adw_navigation_split_view_set_show_content(state.main_window->split_view, true);
}

static void on_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, ListData *data) {
  const GdkRGBA *color_rgba = gtk_color_dialog_button_get_rgba(btn);
  char new_color[8];
  gdk_rgba_to_hex_string(color_rgba, new_color);
  errands_data_set_color(data->ical, new_color, true);
  errands_list_data_save(data);
  errands_sync_update_list(data);
}

static void on_action_export_finish_cb(GObject *obj, GAsyncResult *res, ListData *data) {
  g_autoptr(GFile) f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!f) return;
  g_autofree char *path = g_file_get_path(f);
  FILE *file = fopen(path, "w");
  if (!file) {
    errands_window_add_toast(_("Export failed"));
    return;
  }
  autofree char *ical = icalcomponent_as_ical_string(data->ical);
  fprintf(file, "%s", ical);
  fclose(file);
  errands_window_add_toast(_("Exported"));
  LOG("Export task list %s", data->uid);
}

static void on_action_export(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row) {
  gtk_popover_popdown(row->popover);
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  const char *filename = tmp_str_printf("%s.ics", row->data->uid);
  g_object_set(dialog, "initial-name", filename, NULL);
  gtk_file_dialog_save(dialog, GTK_WINDOW(state.main_window), NULL, (GAsyncReadyCallback)on_action_export_finish_cb,
                       row->data);
}

static void on_action_rename(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row) {
  gtk_popover_popdown(row->popover);
  errands_rename_list_dialog_show(row);
}

static void on_action_delete_completed(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row) {
  gtk_popover_popdown(row->popover);
  errands_task_list_delete_completed(state.main_window->task_list, row->data);
}

static void on_action_delete_cancelled(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row) {
  gtk_popover_popdown(row->popover);
  errands_task_list_delete_cancelled(state.main_window->task_list, row->data);
}

static void on_action_delete(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row) {
  gtk_popover_popdown(row->popover);
  errands_delete_list_dialog_show(row);
}

// --- DND --- //

static guint on_drop_motion_ctrl_enter_timeout_cb(ErrandsSidebarTaskListRow *self) {
  if (!gtk_drop_controller_motion_contains_pointer(self->drop_motion_ctrl)) return G_SOURCE_REMOVE;
  if (errands_sidebar_row_is_selected(self)) return G_SOURCE_REMOVE;
  gtk_widget_activate(GTK_WIDGET(self));

  return G_SOURCE_REMOVE;
}

static void on_drop_motion_ctrl_enter_cb(ErrandsSidebarTaskListRow *self) {
  g_timeout_add_once(500, (GSourceOnceFunc)on_drop_motion_ctrl_enter_timeout_cb, self);
}

static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y,
                           ErrandsSidebarTaskListRow *self) {
  ErrandsTaskItem *drop_item = g_value_get_object(value);
  TaskData *drop_data = errands_task_item_get_data(drop_item);
  ListData *list_data = self->data;

  bool changing_list = drop_data->list != list_data;

  ErrandsTaskItem *drop_item_parent = errands_task_item_get_parent(drop_item);

  // Don't move toplevel task in the same list
  if (!changing_list && !drop_item_parent) return false;

  // Get old parent task
  ErrandsTask *old_parent_task = NULL;
  if (drop_data->parent && drop_item_parent) g_object_get(drop_item_parent, "task-widget", &old_parent_task, NULL);

  // Move data
  ListData *old_list = drop_data->list;
  errands_data_set_parent(drop_data->ical, NULL);
  // Move ical
  if (changing_list) {
    icalcomponent *drop_data_dup_ical = icalcomponent_new_clone(drop_data->ical);
    icalcomponent_remove_component(old_list->ical, drop_data->ical);
    icalcomponent_add_component(list_data->ical, drop_data_dup_ical);
    drop_data->ical = drop_data_dup_ical;
    drop_data->list = list_data;
  }
  // Move TaskData
  GPtrArray *parent_arr = drop_data->parent ? drop_data->parent->children : old_list->children;
  guint idx;
  g_ptr_array_find(parent_arr, drop_data, &idx);
  drop_data = g_ptr_array_steal_index_fast(parent_arr, idx);
  g_ptr_array_add(list_data->children, drop_data);
  // Move sub-tasks
  if (changing_list) {
    g_autoptr(GPtrArray) children = g_ptr_array_sized_new(drop_data->children->len);
    errands_task_data_get_flat_list(drop_data, children);
    for_range(i, 0, children->len) {
      TaskData *child = g_ptr_array_index(children, i);
      icalcomponent *child_data_dup_ical = icalcomponent_new_clone(child->ical);
      icalcomponent_remove_component(old_list->ical, child->ical);
      icalcomponent_add_component(list_data->ical, child_data_dup_ical);
      child->ical = child_data_dup_ical;
      child->list = list_data;
    }
    errands_list_data_save(old_list);
  }
  errands_list_data_save(list_data);

  // Update task model if necessary
  if (drop_data->parent) {
    // Add the item to the current task model
    g_list_store_append(state.main_window->task_list->task_model, drop_item);
    GListModel *parent_model = errands_task_item_get_children_model(drop_item_parent);
    guint idx;
    g_list_store_find(G_LIST_STORE(parent_model), drop_item, &idx);
    g_list_store_remove(G_LIST_STORE(parent_model), idx);
    if (drop_item_parent) g_object_notify(G_OBJECT(drop_item_parent), "children-model-is-empty");
    // Set parent to NULL
    drop_data->parent = NULL;
    errands_task_item_set_parent(drop_item, NULL);
  }

  // Update filter
  bool selected = errands_sidebar_row_is_selected(self);
  errands_task_list_filter(state.main_window->task_list,
                           selected ? GTK_FILTER_CHANGE_MORE_STRICT : GTK_FILTER_CHANGE_DIFFERENT);
  if (old_parent_task) errands_task_update_progress(old_parent_task);

  // Update the UI after the operation
  errands_sidebar_task_list_row_update(errands_sidebar_task_list_row_get(old_list));
  errands_sidebar_task_list_row_update(self);

  return true;
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

static void on_action_print(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row) {
  gtk_popover_popdown(row->popover);
  LOG("Start printing of the list '%s'", row->data->uid);
  TODO("PRINT");
  // g_autofree gchar *str = list_data_print(row->data);
  // start_print(str);
}
