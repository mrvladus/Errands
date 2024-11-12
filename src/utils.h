#pragma once

#include "utils/datetime.h"
#include "utils/files.h"
#include "utils/ical.h"
#include "utils/macros.h"
#include "utils/str.h"

#include "glib.h"
#include <ctype.h>
#include <gtk/gtk.h>
#include <stdint.h>

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

static inline bool string_contains(const char *haystack, const char *needle) {
  return (bool)strcasestr(haystack, needle);
}

// Check if any string in the array contains string
static inline bool string_array_contains(GPtrArray *str_arr, const char *needle) {
  for (int i = 0; i < str_arr->len; i++)
    if (string_contains((char *)str_arr->pdata[i], needle))
      return true;
  return false;
}

// Function to trim leading and trailing whitespace
static inline char *string_trim(char *str) {
  // Trim leading whitespace
  while (isspace((unsigned char)*str))
    str++;
  // Trim trailing whitespace
  char *end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;
  // Null terminate the trimmed string
  *(end + 1) = '\0';
  return str;
}

static inline void generate_hex(char *color) {
  srand(time(NULL));
  int red = rand() % 256;
  int green = rand() % 256;
  int blue = rand() % 256;
  sprintf(color, "#%02X%02X%02X", red, green, blue);
  LOG("Generate color '%s'", color);
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
static inline void errands_add_shortcut(GtkWidget *widget, const char *trigger,
                                        const char *action) {
  GtkEventController *ctrl = gtk_shortcut_controller_new();
  gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(ctrl), GTK_SHORTCUT_SCOPE_GLOBAL);
  GtkShortcut *sc = gtk_shortcut_new(gtk_shortcut_trigger_parse_string(trigger),
                                     gtk_shortcut_action_parse_string(action));
  gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ctrl), sc);
  gtk_widget_add_controller(widget, ctrl);
}

static inline void generate_uuid(char *uuid) {
  gchar *uid = g_uuid_string_random();
  strcpy(uuid, uid);
  g_free(uid);
}

static inline void g_ptr_array_move_before(GPtrArray *array, gpointer element, gpointer target) {
  gint index = -1;
  gint target_index = -1;

  // Find the indices of the element to move and the target element
  for (gint i = 0; i < array->len; i++) {
    if (g_ptr_array_index(array, i) == element) {
      index = i;
    }
    if (g_ptr_array_index(array, i) == target) {
      target_index = i;
    }
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
    if (g_ptr_array_index(array, i) == element) {
      index = i;
    }
    if (g_ptr_array_index(array, i) == target) {
      target_index = i;
    }
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
