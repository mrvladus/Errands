#include "data.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"
#include "vendor/toolbox.h"
#include "window.h"

#include <libical/ical.h>

static void on_right_click(GtkPopover *popover, gint n_press, gdouble x, gdouble y, GtkGestureClick *ctrl);
static void on_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, ListData *data);
static void on_action_rename(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
static void on_action_export(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
static void on_action_delete(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
static void on_action_print(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
// static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y,
//                                            ErrandsSidebarTaskListRow *row);
// static void on_drag_begin(GtkDragSource *source, GdkDrag *drag, ErrandsSidebarTaskListRow *row);
// static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y,
//                         ErrandsSidebarTaskListRow *row);
// static GdkDragAction on_hover_begin(GtkDropTarget *target, gdouble x, gdouble y,
//                                     ErrandsSidebarTaskListRow *row);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_task_list_row_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW);
  G_OBJECT_CLASS(errands_sidebar_task_list_row_parent_class)->dispose(gobject);
}

static void errands_sidebar_task_list_row_class_init(ErrandsSidebarTaskListRowClass *class) {
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/sidebar-task-list-row.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarTaskListRow, color_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarTaskListRow, counter);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSidebarTaskListRow, label);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_right_click);
  G_OBJECT_CLASS(class)->dispose = errands_sidebar_task_list_row_dispose;
}

static void errands_sidebar_task_list_row_init(ErrandsSidebarTaskListRow *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
  // Actions
  errands_add_actions(GTK_WIDGET(self), "task-list-row", "rename", on_action_rename, self, "delete", on_action_delete,
                      self, "print", on_action_print, self, "export", on_action_export, self, NULL);

  // DND
  // GtkDragSource *drag_source = gtk_drag_source_new();
  // gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
  // g_signal_connect(drag_source, "prepare", G_CALLBACK(on_drag_prepare), self);
  // g_signal_connect(drag_source, "drag-begin", G_CALLBACK(on_drag_begin), self);
  // gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(drag_source));

  // Drop target setup
  // GtkDropTarget *drop_target = gtk_drop_target_new(G_TYPE_OBJECT, GDK_ACTION_MOVE);
  // g_signal_connect(drop_target, "drop", G_CALLBACK(on_drop), self);
  // g_signal_connect(drop_target, "enter", G_CALLBACK(on_hover_begin), self);
  // gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(drop_target));

  // Drop motion
  // self->hover_ctrl = gtk_drop_controller_motion_new();
  // g_signal_connect(drop_target, "enter", G_CALLBACK(on_hover_begin), self);
  // gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(self->hover_ctrl));
}

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_new(ListData *data) {
  tb_log("Task List Row '%s': Create", errands_data_get_str(data, DATA_PROP_LIST_UID));
  ErrandsSidebarTaskListRow *row = g_object_new(ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW, NULL);
  row->data = data;
  // Set color
  GdkRGBA color;
  gdk_rgba_parse(&color, errands_data_get_str(data, DATA_PROP_COLOR));
  gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(row->color_btn), &color);
  g_signal_connect(row->color_btn, "notify::rgba", G_CALLBACK(on_color_changed), data);
  // Update
  errands_sidebar_task_list_row_update_title(row);
  errands_sidebar_task_list_row_update_counter(row);
  return row;
}

// ---------- PUBLIC FUNCTIONS ---------- //

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(const char *uid) {
  GPtrArray *children = get_children(state.main_window->sidebar->task_lists_box);
  for (size_t i = 0; i < children->len; i++) {
    ListData *data = ((ErrandsSidebarTaskListRow *)children->pdata[i])->data;
    if (g_str_equal(errands_data_get_str(data, DATA_PROP_LIST_UID), uid)) return children->pdata[i];
  }
  g_ptr_array_free(children, false);
  return NULL;
}

void errands_sidebar_task_list_row_update_counter(ErrandsSidebarTaskListRow *row) {
  size_t c = 0;
  GPtrArray *tasks = list_data_get_tasks(row->data);
  for (size_t i = 0; i < tasks->len; i++) {
    TaskData *td = tasks->pdata[i];
    bool deleted = errands_data_get_bool(td, DATA_PROP_DELETED);
    bool trash = errands_data_get_bool(td, DATA_PROP_TRASH);
    bool completed = !icaltime_is_null_time(errands_data_get_time(td, DATA_PROP_COMPLETED_TIME));
    if (!deleted && !trash && !completed) c++;
  }
  char num[32];
  sprintf(num, "%zu", c);
  gtk_label_set_label(GTK_LABEL(row->counter), c > 0 ? num : "");
  g_ptr_array_free(tasks, false);
}

void errands_sidebar_task_list_row_update_title(ErrandsSidebarTaskListRow *row) {
  gtk_label_set_label(GTK_LABEL(row->label), errands_data_get_str(row->data, DATA_PROP_LIST_NAME));
}

// ---------- CALLBACKS ---------- //

void on_errands_sidebar_task_list_row_activate(GtkListBox *box, ErrandsSidebarTaskListRow *row, gpointer user_data) {
  ErrandsTaskList *task_list = state.main_window->task_list;
  // Unselect filter rows
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.main_window->sidebar->filters_box));
  // Set setting
  const char *list_uid = tb_tmp_str_printf(errands_data_get_str(row->data, DATA_PROP_LIST_UID));
  errands_settings_set_string("last_list_uid", list_uid);
  errands_task_list_show_task_list(task_list, row->data);
  tb_log("Switch to list '%s'", list_uid);
}

static void on_right_click(GtkPopover *popover, gint n_press, gdouble x, gdouble y, GtkGestureClick *ctrl) {
  gtk_popover_set_pointing_to(popover, &(GdkRectangle){.x = x, .y = y});
  gtk_popover_popup(popover);
}

static void on_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, ListData *data) {
  const GdkRGBA *color_rgba = gtk_color_dialog_button_get_rgba(btn);
  char new_color[8];
  gdk_rgba_to_hex_string(color_rgba, new_color);
  errands_data_set_str(data, DATA_PROP_COLOR, new_color);
  errands_data_write_list(data);
}

static void on_action_rename(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row) {
  errands_sidebar_rename_list_dialog_show(row);
}

// - EXPORT - //

static void on_action_export_finish_cb(GObject *obj, GAsyncResult *res, gpointer data) {
  g_autoptr(GFile) f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!f) return;
  g_autofree char *path = g_file_get_path(f);
  FILE *file = fopen(path, "w");
  if (!file) {
    errands_window_add_toast(state.main_window, "Export failed");
    return;
  }
  char *ical = icalcomponent_as_ical_string(data);
  fprintf(file, "%s", ical);
  fclose(file);
  free(ical);
  tb_log("Export task list %s", errands_data_get_str(data, DATA_PROP_LIST_UID));
}

static void on_action_export(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  const char *filename = tb_tmp_str_printf("%s.ics", errands_data_get_str(row->data, DATA_PROP_LIST_UID));
  g_object_set(dialog, "initial-name", filename, NULL);
  gtk_file_dialog_save(dialog, GTK_WINDOW(state.main_window), NULL, on_action_export_finish_cb, row->data);
}

static void on_action_delete(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row) {
  errands_sidebar_delete_list_dialog_show(row);
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
  tb_log("Start printing of the list '%s'", errands_data_get_str(row->data, DATA_PROP_LIST_UID));
  g_autofree gchar *str = list_data_print(row->data);
  start_print(str);
}

// // --- DND --- //

// static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y,
//                                            ErrandsSidebarTaskListRow *row) {
//   GValue value = G_VALUE_INIT;
//   g_value_init(&value, G_TYPE_OBJECT);
//   g_value_set_object(&value, row);
//   return gdk_content_provider_new_for_value(&value);
// }

// static void on_drag_begin(GtkDragSource *source, GdkDrag *drag, ErrandsSidebarTaskListRow *row) {
//   g_object_set(gtk_drag_icon_get_for_drag(drag), "child",
//                g_object_new(GTK_TYPE_BUTTON, "label", row->data->name, NULL), NULL);
// }

// static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y,
//                         ErrandsSidebarTaskListRow *target_row) {
//   gpointer object = g_value_get_object(value);
//   if (ERRANDS_IS_SIDEBAR_TASK_LIST_ROW(object)) {
//     ErrandsSidebarTaskListRow *row = ERRANDS_SIDEBAR_TASK_LIST_ROW(object);
//     if (row == target_row)
//       return false;
//     tb_log("Reorder task lists");
//     // Move widget
//     int idx = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(target_row));
//     GtkListBox *box = (GtkListBox *)gtk_widget_get_parent(GTK_WIDGET(row));
//     gtk_list_box_remove(box, GTK_WIDGET(row));
//     gtk_list_box_insert(box, GTK_WIDGET(row), idx);
//     // Move data
//     guint idx_to_move, idx_to_move_before;
//     g_ptr_array_find(ldata, row->data, &idx_to_move);
//     TaskListData *to_move = g_ptr_array_steal_index(ldata, idx_to_move);
//     g_ptr_array_find(ldata, target_row->data, &idx_to_move_before);
//     if (idx_to_move < idx_to_move_before)
//       g_ptr_array_insert(ldata, idx_to_move_before + 1, to_move);
//     else
//       g_ptr_array_insert(ldata, idx_to_move_before, to_move);
//     errands_data_write();
//     return true;
//   } else if (ERRANDS_IS_TASK(object)) {
//     ErrandsTask *task = ERRANDS_TASK(object);
//     if (!strcmp(task->data->list_uid, target_row->data->uid))
//       return false;
//     tb_log("Move task '%s' to list '%s'", task->data->uid, target_row->data->uid);
//     ErrandsSidebarTaskListRow *task_row =
//     errands_sidebar_task_list_row_get(task->data->list_uid);
//     // Move widget
//     if (!strcmp(task->data->parent, "")) {
//       GtkWidget *first_task = gtk_widget_get_first_child(state.task_list->task_list);
//       if (first_task)
//         gtk_widget_insert_before(GTK_WIDGET(task), state.task_list->task_list, first_task);
//     } else {
//       ErrandsTask *task_parent = ERRANDS_TASK(
//           gtk_widget_get_ancestor(gtk_widget_get_parent(GTK_WIDGET(task)), ERRANDS_TYPE_TASK));
//       gtk_box_remove(GTK_BOX(task_parent->sub_tasks), GTK_WIDGET(task));
//       gtk_box_prepend(GTK_BOX(state.task_list->task_list), GTK_WIDGET(task));
//       errands_task_update_progress(task_parent);
//     }
//     gtk_widget_set_visible(GTK_WIDGET(task), false);
//     // Change data
//     strcpy(task->data->list_uid, target_row->data->uid);
//     strcpy(task->data->parent, "");
//     GPtrArray* sub_tasks = errands_task_get_sub_tasks(task);
//     for (int i = 0; i < sub_tasks->len; i++) {
//       ErrandsTask *t = sub_tasks->pdata[i];
//       strcpy(t->data->list_uid, target_row->data->uid);
//     }
//     errands_data_write();
//     // Update ui
//     errands_sidebar_task_list_row_update_counter(task_row);
//     errands_sidebar_task_list_row_update_counter(target_row);
//     errands_task_list_update_title();
//     // TODO: sync
//     return true;
//   }

//   return false;
// }

// // static time_t hover_time = 0;
// // static GdkDragAction on_hover_begin(GtkDropTarget *target, gdouble x, gdouble y,
// //                                     ErrandsSidebarTaskListRow *row) {
// //   if (time(NULL) - hover_time > 1) {
// //     bool contains =
// //         gtk_event_controller_motion_contains_pointer((GtkEventControllerMotion
// //         *)row->hover_ctrl);
// //   }
// //   return 0;
// // }
