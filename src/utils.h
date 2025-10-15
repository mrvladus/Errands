#pragma once

#include "vendor/toolbox.h"

#include <gtk/gtk.h>

#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>

#define RUN_THREAD_FUNC(func, on_finish_cb)                                                                            \
  g_autoptr(GTask) task = g_task_new(NULL, NULL, on_finish_cb, NULL);                                                  \
  g_task_run_in_thread(task, func);

// Get children of the widget
static inline GPtrArray *get_children(GtkWidget *parent) {
  GPtrArray *children = g_ptr_array_new();
  GtkWidget *first_child = gtk_widget_get_first_child(parent);
  if (first_child) {
    g_ptr_array_add(children, first_child);
    GtkWidget *next_child = gtk_widget_get_next_sibling(first_child);
    while (next_child) {
      g_ptr_array_add(children, next_child);
      next_child = gtk_widget_get_next_sibling(next_child);
    }
  }
  return children;
}

static inline void gtk_box_remove_all(GtkWidget *box) {
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child(box))) gtk_box_remove(GTK_BOX(box), child);
}

static inline bool string_contains(const char *haystack, const char *needle) {
  return (bool)strcasestr(haystack, needle);
}

// Check if any string in the array contains string
static inline bool string_array_contains(GPtrArray *str_arr, const char *needle) {
  for (int i = 0; i < str_arr->len; i++)
    if (string_contains((char *)str_arr->pdata[i], needle)) return true;
  return false;
}

// Function to trim leading and trailing whitespace
static inline char *string_trim(char *str) {
  // Trim leading whitespace
  while (isspace((unsigned char)*str)) str++;
  // Trim trailing whitespace
  char *end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) end--;
  // Null terminate the trimmed string
  *(end + 1) = '\0';
  return str;
}

static inline const char *generate_hex() {
  static char color[8];
  srand(time(NULL));
  int red = rand() % 256;
  int green = rand() % 256;
  int blue = rand() % 256;
  sprintf(color, "#%02X%02X%02X", red, green, blue);
  tb_log("Generate color '%s'", color);
  return color;
}

static inline void gdk_rgba_to_hex_string(const GdkRGBA *rgba, char *hex_string) {
  // Convert the RGBA components to integers in the range [0, 255]
  int r = (int)(rgba->red * 255);
  int g = (int)(rgba->green * 255);
  int b = (int)(rgba->blue * 255);
  // Format the string as #RRGGBB
  sprintf(hex_string, "#%02X%02X%02X", r, g, b);
}

// Add shortcut to the widget
static inline void errands_add_shortcut(GtkWidget *widget, const char *trigger, const char *action) {
  GtkEventController *ctrl = gtk_shortcut_controller_new();
  gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(ctrl), GTK_SHORTCUT_SCOPE_GLOBAL);
  GtkShortcut *sc =
      gtk_shortcut_new(gtk_shortcut_trigger_parse_string(trigger), gtk_shortcut_action_parse_string(action));
  gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ctrl), sc);
  gtk_widget_add_controller(widget, ctrl);
}

static inline void generate_uuid(char *uuid) {
  g_autofree gchar *uid = g_uuid_string_random();
  strcpy(uuid, uid);
}

static inline void g_ptr_array_move_before(GPtrArray *array, gpointer element, gpointer target) {
  gint index = -1;
  gint target_index = -1;

  // Find the indices of the element to move and the target element
  for (gint i = 0; i < array->len; i++) {
    if (g_ptr_array_index(array, i) == element) { index = i; }
    if (g_ptr_array_index(array, i) == target) { target_index = i; }
  }

  // Check if both indices were found
  if (index == -1 || target_index == -1 || index == target_index) {
    g_print("Element or target not found, or they are the same.\n");
    return;
  }

  // Remove the element from its current position
  gpointer temp = g_ptr_array_index(array, index);
  g_ptr_array_remove_index(array, index);

  // Adjust the target index if the element was before it
  if (index < target_index) {
    target_index--; // Adjust because we removed the element
  }

  // Insert the element before the target
  g_ptr_array_insert(array, target_index, temp);
}

static inline void g_ptr_array_move_after(GPtrArray *array, gpointer element, gpointer target) {
  gint index = -1;
  gint target_index = -1;

  // Find the indices of the element to move and the target element
  for (gint i = 0; i < array->len; i++) {
    if (g_ptr_array_index(array, i) == element) { index = i; }
    if (g_ptr_array_index(array, i) == target) { target_index = i; }
  }

  // Check if both indices were found
  if (index == -1 || target_index == -1 || index == target_index) {
    g_print("Element or target not found, or they are the same.\n");
    return;
  }

  // Remove the element from its current position
  gpointer temp = g_ptr_array_index(array, index);
  g_ptr_array_remove_index(array, index);

  // Adjust the target index if the element was before it
  if (index < target_index) {
    target_index--; // Adjust because we removed the element
  }

  // Insert the element after the target
  g_ptr_array_insert(array, target_index + 1, temp);
}

static inline int *string_to_int_array(const char *str) {
  // First, count the number of integers
  int count = 1;
  for (const char *p = str; *p != '\0'; p++) {
    if (*p == ',') count++;
  }

  // Allocate memory for the array, with one extra space for NULL termination
  int *arr = (int *)malloc((count + 1) * sizeof(int));
  if (!arr) return NULL;

  // Parse the string and fill the array
  int index = 0;
  const char *start = str;
  char *end;
  while (*start != '\0') {
    arr[index++] = strtol(start, &end, 10);
    start = end + 1;
  }

  arr[index] = 0; // NULL terminate the array
  return arr;
}

static inline GStrv gstrv_remove_duplicates(GStrv strv) {
  g_autoptr(GHashTable) hash_table = g_hash_table_new(g_str_hash, g_str_equal);
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for (int i = 0; strv[i] != NULL; i++)
    if (!g_hash_table_contains(hash_table, strv[i])) {
      g_hash_table_add(hash_table, strv[i]);
      g_strv_builder_add(builder, strv[i]);
    }
  return g_strv_builder_end(builder);
}

static inline void errands_add_actions(GtkWidget *widget, const char *action_group_name, const char *action_name1,
                                       void *cb1, void *data1, ...) {
  g_autoptr(GSimpleActionGroup) ag = g_simple_action_group_new();
  // Create the first action
  g_autoptr(GSimpleAction) action = g_simple_action_new(action_name1, NULL);
  g_signal_connect(action, "activate", G_CALLBACK(cb1), data1);
  g_action_map_add_action(G_ACTION_MAP(ag), G_ACTION(action));
  // Handle additional actions
  va_list args;
  va_start(args, data1);
  const char *action_name;
  void *callback;
  void *data;
  while ((action_name = va_arg(args, const char *)) != NULL) {
    callback = va_arg(args, void *);
    data = va_arg(args, void *);
    // Create and add the new action
    g_autoptr(GSimpleAction) new_action = g_simple_action_new(action_name, NULL);
    g_signal_connect(new_action, "activate", G_CALLBACK(callback), data);
    g_action_map_add_action(G_ACTION_MAP(ag), G_ACTION(new_action));
  }
  va_end(args);
  // Insert the action group into the widget
  gtk_widget_insert_action_group(GTK_WIDGET(widget), action_group_name, G_ACTION_GROUP(ag));
}
