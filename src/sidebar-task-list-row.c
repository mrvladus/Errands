#include "sidebar-task-list-row.h"
#include "adwaita.h"
#include "components.h"
#include "data.h"
#include "delete-list-dialog.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "pango/pango-layout.h"
#include "rename-list-dialog.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

#include <stdbool.h>
#include <string.h>

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x,
                           gdouble y, GtkPopover *popover);
static void on_action_rename(GSimpleAction *action, GVariant *param,
                             ErrandsSidebarTaskListRow *row);
static void on_action_export(GSimpleAction *action, GVariant *param,
                             gpointer data);
static void on_action_delete(GSimpleAction *action, GVariant *param,
                             ErrandsSidebarTaskListRow *row);
static void on_action_print(GSimpleAction *action, GVariant *param,
                            ErrandsSidebarTaskListRow *row);

G_DEFINE_TYPE(ErrandsSidebarTaskListRow, errands_sidebar_task_list_row,
              GTK_TYPE_LIST_BOX_ROW)

static void errands_sidebar_task_list_row_class_init(
    ErrandsSidebarTaskListRowClass *class) {}

static void
errands_sidebar_task_list_row_init(ErrandsSidebarTaskListRow *self) {
  // Box
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(self), box);

  // Color
  // TODO

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
  GMenu *menu = errands_menu_new(
      4, "Rename", "task-list-row.rename", "Delete", "task-list-row.delete",
      "Export", "task-list-row.export", "Print", "task-list-row.print");

  // Menu popover
  GtkWidget *menu_popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
  g_object_set(menu_popover, "has-arrow", false, "halign", GTK_ALIGN_START,
               NULL);
  gtk_box_append(GTK_BOX(box), menu_popover);
  GtkGesture *ctrl = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(ctrl), 3);
  g_signal_connect(ctrl, "released", G_CALLBACK(on_right_click), menu_popover);
  gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(ctrl));

  // Actions
  GSimpleActionGroup *ag = errands_action_group_new(
      3, "rename", on_action_rename, self, "delete", on_action_delete, self,
      "print", on_action_print, self);
  gtk_widget_insert_action_group(GTK_WIDGET(self), "task-list-row",
                                 G_ACTION_GROUP(ag));
}

ErrandsSidebarTaskListRow *
errands_sidebar_task_list_row_new(TaskListData *data) {
  ErrandsSidebarTaskListRow *row =
      g_object_new(ERRANDS_TYPE_SIDEBAR_TASK_LIST_ROW, NULL);
  row->data = data;
  errands_sidebar_task_list_row_update_title(row);
  errands_sidebar_task_list_row_update_counter(row);
  return row;
}

ErrandsSidebarTaskListRow *errands_sidebar_task_list_row_get(const char *uid) {
  GPtrArray *children = get_children(state.sidebar->task_lists_box);
  for (int i = 0; i < children->len; i++) {
    TaskListData *l_data =
        ((ErrandsSidebarTaskListRow *)children->pdata[i])->data;
    if (!strcmp(l_data->uid, uid))
      return children->pdata[i];
  }
  return NULL;
}

void errands_sidebar_task_list_row_update_counter(
    ErrandsSidebarTaskListRow *row) {
  int c = 0;
  for (int i = 0; i < state.t_data->len; i++) {
    TaskData *td = state.t_data->pdata[i];
    if (!strcmp(row->data->uid, td->list_uid) && !td->trash && !td->deleted &&
        !td->completed)
      c++;
  }
  char *num = g_strdup_printf("%d", c);
  gtk_label_set_label(GTK_LABEL(row->counter), c > 0 ? num : "");
  g_free(num);
}

void errands_sidebar_task_list_row_update_title(
    ErrandsSidebarTaskListRow *row) {
  gtk_label_set_label(GTK_LABEL(row->label), row->data->name);
}

// --- SIGNAL HANDLERS --- //

void on_errands_sidebar_task_list_row_activate(GtkListBox *box,
                                               ErrandsSidebarTaskListRow *row,
                                               gpointer user_data) {
  // Unselect filter rows
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.sidebar->filters_box));

  // Switch to Task List view
  adw_view_stack_set_visible_child_name(
      ADW_VIEW_STACK(state.main_window->stack), "errands_task_list_page");

  // Filter by uid
  errands_task_list_filter_by_uid(row->data->uid);
  state.task_list->data = row->data;

  // Show entry
  gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), true);

  // Update title
  errands_task_list_update_title();

  LOG("Switch to list '%s'", row->data->uid);
}

// --- SIGNALS HANDLERS --- //

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x,
                           gdouble y, GtkPopover *popover) {
  gtk_popover_set_pointing_to(popover, &(GdkRectangle){.x = x, .y = y});
  gtk_popover_popup(popover);
}

static void on_action_rename(GSimpleAction *action, GVariant *param,
                             ErrandsSidebarTaskListRow *row) {
  errands_rename_list_dialog_show(row);
}

static void on_action_export(GSimpleAction *action, GVariant *param,
                             gpointer data) {
  LOG("Export");
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
static void begin_print(GtkPrintOperation *operation, GtkPrintContext *context,
                        gpointer user_data) {
  const char *text = (const char *)user_data;
  int num_lines = 0;
  const char *p = text;
  while ((p = strchr(p, '\n'))) {
    num_lines++;
    p++;
  }
  int total_pages = (num_lines + LINES_PER_PAGE - 1) / LINES_PER_PAGE;
  gtk_print_operation_set_n_pages(operation, total_pages);
}

// Function to draw the text, handling LINES_PER_PAGE
static void print_draw_page(GtkPrintOperation *operation,
                            GtkPrintContext *context, int page_nr,
                            gpointer user_data) {
  cairo_t *cr = gtk_print_context_get_cairo_context(context);
  const char *text = (const char *)user_data;
  // Set the font to monospace and size to 12
  cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
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
      y += LINE_HEIGHT; // Move down for the next line (LINE_HEIGHT between
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
  g_signal_connect(print, "draw-page", G_CALLBACK(print_draw_page),
                   (gpointer)str);
  g_signal_connect(print, "begin-print", G_CALLBACK(begin_print),
                   (gpointer)str);
  // Set default print settings if needed
  GtkPrintOperationResult result =
      gtk_print_operation_run(print, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                              GTK_WINDOW(state.main_window), NULL);
  // Check the result (if user cancels or accepts the dialog)
  if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
    g_print("An error occurred during the print operation.\n");
  } else if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
    g_print("Print operation successful.\n");
  }
  // Cleanup the print operation object
  g_object_unref(print);
}

static void on_action_print(GSimpleAction *action, GVariant *param,
                            ErrandsSidebarTaskListRow *row) {
  LOG("Start printing of the list '%s'", row->data->uid);

  GString *str = errands_data_print_list(row->data->uid);
  printf("%s", str->str);
  start_print(str->str);
  g_string_free(str, true);
}
