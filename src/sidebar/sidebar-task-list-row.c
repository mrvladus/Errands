#include "sidebar-task-list-row.h"
#include "../components.h"
#include "../data.h"
#include "../dialogs/delete-list-dialog.h"
#include "../dialogs/rename-list-dialog.h"
#include "../state.h"
#include "../task-list.h"
#include "../utils.h"
#include "gdk/gdk.h"
#include "glib-object.h"
#include "gtk/gtk.h"

#include <glib/gi18n.h>

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y,
                           GtkPopover *popover);
static void on_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, TaskListData *data);
static void on_action_rename(GSimpleAction *action, GVariant *param,
                             ErrandsSidebarTaskListRow *row);
static void on_action_export(GSimpleAction *action, GVariant *param,
                             ErrandsSidebarTaskListRow *row);
static void on_action_delete(GSimpleAction *action, GVariant *param,
                             ErrandsSidebarTaskListRow *row);
static void on_action_print(GSimpleAction *action, GVariant *param, ErrandsSidebarTaskListRow *row);
static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y,
                                           ErrandsSidebarTaskListRow *row);
static void on_drag_begin(GtkDragSource *source, GdkDrag *drag, ErrandsSidebarTaskListRow *row);
static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y,
                        ErrandsSidebarTaskListRow *row);

G_DEFINE_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row, GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_task_list_row_class_init(ErrandsSidebarTaskListRowClass *class) {}

static void errands_sidebar_task_list_row_init(ErrandsSidebarTaskListRow *self) {
  gtk_widget_add_css_class(GTK_WIDGET(self), "sidebar-task-list-row");

  // Box
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(self), box);

  // Color
  self->color_dialog = gtk_color_dialog_new();
  self->color_btn = gtk_color_dialog_button_new(self->color_dialog);
  gtk_box_append(GTK_BOX(box), self->color_btn);

  // Label
  self->label = gtk_label_new("");
  gtk_box_append(GTK_BOX(box), self->label);
  gtk_label_set_ellipsize(GTK_LABEL(self->label), PANGO_ELLIPSIZE_END);

  // Counter
  self->counter = gtk_label_new("");
  gtk_box_append(GTK_BOX(box), self->counter);
  g_object_set(self->counter, "hexpand", true, "halign", GTK_ALIGN_END, NULL);
  gtk_widget_add_css_class(self->counter, "dim-label");
  gtk_widget_add_css_class(self->counter, "caption");

  // Right-click menu
  GMenu *menu =
      errands_menu_new(4, _("Rename"), "task-list-row.rename", _("Delete"), "task-list-row.delete",
                       _("Export"), "task-list-row.export", _("Print"), "task-list-row.print");

  // Menu popover
  GtkWidget *menu_popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
  g_object_set(menu_popover, "has-arrow", false, "halign", GTK_ALIGN_START, NULL);
  gtk_box_append(GTK_BOX(box), menu_popover);
  GtkGesture *ctrl = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(ctrl), 3);
  g_signal_connect(ctrl, "released", G_CALLBACK(on_right_click), menu_popover);
  gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(ctrl));

  // Actions
  GSimpleActionGroup *ag = errands_action_group_new(
      4, "rename", on_action_rename, self, "delete", on_action_delete, self, "print",
      on_action_print, self, "export", on_action_export, self);
  gtk_widget_insert_action_group(GTK_WIDGET(self), "task-list-row", G_ACTION_GROUP(ag));

  // DND
  GtkDragSource *drag_source = gtk_drag_source_new();
  gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
  g_signal_connect(drag_source, "prepare", G_CALLBACK(on_drag_prepare), self);
  g_signal_connect(drag_source, "drag-begin", G_CALLBACK(on_drag_begin), self);
  gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(drag_source));

  // Drop target setup
  GtkDropTarget *drop_target = gtk_drop_target_new(G_TYPE_OBJECT, GDK_ACTION_MOVE);
  g_signal_connect(drop_target, "drop", G_CALLBACK(on_drop), self);
  gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(drop_target));
}

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_new(TaskListData *data) {
  ErrandsSidebarTaskListRow *row = g_object_new(ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW, NULL);
  row->data = data;
  // Set color
  GdkRGBA color;
  gdk_rgba_parse(&color, data->color);
  gtk_color_dialog_button_set_rgba(GTK_COLOR_DIALOG_BUTTON(row->color_btn), &color);
  g_signal_connect(row->color_btn, "notify::rgba", G_CALLBACK(on_color_changed), data);
  // Update
  errands_sidebar_task_list_row_update_title(row);
  errands_sidebar_task_list_row_update_counter(row);
  return row;
}

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(const char *uid) {
  GPtrArray *children = get_children(state.sidebar->task_lists_box);
  for (int i = 0; i < children->len; i++) {
    TaskListData *l_data = ((ErrandsSidebarTaskListRow *)children->pdata[i])->data;
    if (!strcmp(l_data->uid, uid))
      return children->pdata[i];
  }
  return NULL;
}

void errands_sidebar_task_list_row_update_counter(ErrandsSidebarTaskListRow *row) {
  int c = 0;
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!strcmp(row->data->uid, td->list_uid) && !td->trash && !td->deleted && !td->completed)
      c++;
  }
  char *num = g_strdup_printf("%d", c);
  gtk_label_set_label(GTK_LABEL(row->counter), c > 0 ? num : "");
  g_free(num);
}

void errands_sidebar_task_list_row_update_title(ErrandsSidebarTaskListRow *row) {
  gtk_label_set_label(GTK_LABEL(row->label), row->data->name);
}

// --- SIGNAL HANDLERS --- //

void on_errands_sidebar_task_list_row_activate(GtkListBox *box, ErrandsSidebarTaskListRow *row,
                                               gpointer user_data) {
  // Unselect filter rows
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.sidebar->filters_box));
  // Switch to Task List view
  adw_view_stack_set_visible_child_name(ADW_VIEW_STACK(state.main_window->stack),
                                        "errands_task_list_page");
  // Filter by uid
  errands_task_list_filter_by_uid(row->data->uid);
  state.task_list->data = row->data;
  // Show entry
  gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), true);
  // Update title
  errands_task_list_update_title();

  LOG("Switch to list '%s'", row->data->uid);
}

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y,
                           GtkPopover *popover) {
  gtk_popover_set_pointing_to(popover, &(GdkRectangle){.x = x, .y = y});
  gtk_popover_popup(popover);
}

static void on_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, TaskListData *data) {
  const GdkRGBA *color_rgba = gtk_color_dialog_button_get_rgba(btn);
  free(data->color);
  data->color = gdk_rgba_to_hex_string(color_rgba);
  errands_data_write();
}

static void on_action_rename(GSimpleAction *action, GVariant *param,
                             ErrandsSidebarTaskListRow *row) {
  errands_rename_list_dialog_show(row);
}

// - EXPORT - //

static void __on_export_finish(GObject *obj, GAsyncResult *res, gpointer data) {
  GFile *f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!f)
    return;
  FILE *file = fopen(g_file_get_path(f), "w");
  if (!file) {
    fclose(file);
    return;
  }
  TaskListData *tld = data;
  char *ical = errands_data_task_list_as_ical(tld);
  fprintf(file, "%s", ical);
  fclose(file);
  free(ical);
  LOG("Export task list '%s'", tld->uid);
}

static void on_action_export(GSimpleAction *action, GVariant *param,
                             ErrandsSidebarTaskListRow *row) {
  GtkFileDialog *dialog = gtk_file_dialog_new();
  GString *name = g_string_new(row->data->name);
  g_string_append(name, ".ics");
  g_object_set(dialog, "initial-name", name->str, NULL);
  gtk_file_dialog_save(dialog, GTK_WINDOW(state.main_window), NULL, __on_export_finish, row->data);
  g_string_free(name, true);
}

static void on_action_delete(GSimpleAction *action, GVariant *param,
                             ErrandsSidebarTaskListRow *row) {
  errands_delete_list_dialog_show(row);
}

// - PRINTING - //

#define FONT_SIZE 12
#define LINE_HEIGHT 20
#define LINES_PER_PAGE 38

// Function to calculate number of pages and handle pagination
static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context, const char *text) {
  int num_lines = 0;
  char c;
  int len = 0;
  for (int i = 0; i < strlen(text); i++) {
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
  int total_pages = (num_lines + LINES_PER_PAGE - 1) / LINES_PER_PAGE;
  gtk_print_operation_set_n_pages(operation, total_pages);
}

// Function to draw the text, handling LINES_PER_PAGE
static void print_draw_page(GtkPrintOperation *operation, GtkPrintContext *context, int page_nr,
                            gpointer user_data) {
  cairo_t *cr = gtk_print_context_get_cairo_context(context);
  const char *text = (const char *)user_data;
  // Set the font to monospace and size to 12
  cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, FONT_SIZE);
  // Split the text into lines
  char *text_copy = g_strdup(text); // Duplicate text to safely tokenize
  char *line = strtok(text_copy, "\n");
  // Calculate the start and end lines for the current page
  int start_line = page_nr * LINES_PER_PAGE;
  int end_line = start_line + LINES_PER_PAGE;

  double x = 10; // Starting x position
  double y = 10; // Starting y position for the first line

  // Iterate over each line and draw it if it belongs to the current page
  int current_line = 0;
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
  // Free the duplicated text
  g_free(text_copy);
  // Ensure everything is drawn
  cairo_stroke(cr);
}

void start_print(const char *str) {
  // Create a new print operation
  GtkPrintOperation *print = gtk_print_operation_new();
  // Connect signal to draw on page
  g_signal_connect(print, "draw-page", G_CALLBACK(print_draw_page), (gpointer)str);
  g_signal_connect(print, "begin-print", G_CALLBACK(begin_print), (gpointer)str);
  // Set default print settings if needed
  GtkPrintOperationResult result = gtk_print_operation_run(
      print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, GTK_WINDOW(state.main_window), NULL);
  // Check the result (if user cancels or accepts the dialog)
  if (result == GTK_PRINT_OPERATION_RESULT_ERROR)
    g_print("An error occurred during the print operation.\n");
  else if (result == GTK_PRINT_OPERATION_RESULT_APPLY)
    g_print("Print operation successful.\n");

  // Cleanup the print operation object
  g_object_unref(print);
}

static void on_action_print(GSimpleAction *action, GVariant *param,
                            ErrandsSidebarTaskListRow *row) {
  LOG("Start printing of the list '%s'", row->data->uid);

  GString *str = errands_data_print_list(row->data->uid);
  start_print(str->str);
  g_string_free(str, true);
}

// --- DND --- //

static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y,
                                           ErrandsSidebarTaskListRow *row) {
  GValue value = G_VALUE_INIT;
  g_value_init(&value, G_TYPE_OBJECT);
  g_value_set_object(&value, row);
  return gdk_content_provider_new_for_value(&value);
}

static void on_drag_begin(GtkDragSource *source, GdkDrag *drag, ErrandsSidebarTaskListRow *row) {
  g_object_set(gtk_drag_icon_get_for_drag(drag), "child",
               g_object_new(GTK_TYPE_BUTTON, "label", row->data->name, NULL), NULL);
}

static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y,
                        ErrandsSidebarTaskListRow *target_row) {

  ErrandsSidebarTaskListRow *row = g_value_get_object(value);
  if (!ERRANDS_IS_SIDEBAR_TASK_LIST_ROW(row))
    return false;
  LOG("Reorder task lists");
  // Move widget
  int idx = gtk_list_box_row_get_index(GTK_LIST_BOX_ROW(target_row));
  GtkListBox *box = (GtkListBox *)gtk_widget_get_parent(GTK_WIDGET(row));
  gtk_list_box_remove(box, GTK_WIDGET(row));
  gtk_list_box_insert(box, GTK_WIDGET(row), idx);
  // Move data
  guint idx_to_move, idx_to_move_before;
  g_ptr_array_find(state.tl_data, row->data, &idx_to_move);
  TaskListData *to_move = g_ptr_array_steal_index(state.tl_data, idx_to_move);
  g_ptr_array_find(state.tl_data, target_row->data, &idx_to_move_before);
  if (idx_to_move < idx_to_move_before)
    g_ptr_array_insert(state.tl_data, idx_to_move_before + 1, to_move);
  else
    g_ptr_array_insert(state.tl_data, idx_to_move_before, to_move);
  errands_data_write();
  return true;
}