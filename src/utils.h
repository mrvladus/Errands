#pragma once

#include <gtk/gtk.h>

#include <ctype.h>

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
