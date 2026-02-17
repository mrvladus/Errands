#pragma once

#include "gio/gio.h"
#include <gtk/gtk.h>

#include <ctype.h>
#include <stddef.h>

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

static inline void get_children_to_array(GtkWidget *parent, GPtrArray *children) {
  GtkWidget *first_child = gtk_widget_get_first_child(parent);
  if (first_child) {
    g_ptr_array_add(children, first_child);
    GtkWidget *next_child = gtk_widget_get_next_sibling(first_child);
    while (next_child) {
      g_ptr_array_add(children, next_child);
      next_child = gtk_widget_get_next_sibling(next_child);
    }
  }
}

static inline void get_children_sized(GtkWidget *parent, GtkWidget **children, size_t n) {
  GtkWidget *first_child = gtk_widget_get_first_child(parent);
  if (first_child) {
    children[0] = first_child;
    GtkWidget *next_child = gtk_widget_get_next_sibling(first_child);
    for (size_t i = 1; i < n && next_child; i++) {
      children[i] = next_child;
      next_child = gtk_widget_get_next_sibling(next_child);
    }
  }
}

#define for_child_in_parent(child, parent)                                                                             \
  for (GtkWidget *child = gtk_widget_get_first_child(parent); child; child = gtk_widget_get_next_sibling(child))

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

static inline GSimpleActionGroup *errands_add_action_group(void *widget, const char *group_name) {
  g_autoptr(GSimpleActionGroup) ag = g_simple_action_group_new();
  gtk_widget_insert_action_group(GTK_WIDGET(widget), group_name, G_ACTION_GROUP(ag));
  return ag;
}

static inline void errands_add_action(GSimpleActionGroup *ag, const char *name, void *cb, void *data,
                                      const char *param_str) {
  g_autoptr(GVariantType) vtype = param_str ? g_variant_type_new(param_str) : NULL;
  g_autoptr(GSimpleAction) action = g_simple_action_new(name, vtype);
  g_signal_connect(action, "activate", G_CALLBACK(cb), data);
  g_action_map_add_action(G_ACTION_MAP(ag), G_ACTION(action));
}

static inline gchar *str_to_markup(const char *str) {
  if (!str) return NULL;

  g_autoptr(GError) error = NULL;
  g_autofree gchar *escaped_text = g_markup_escape_text(str, -1);
  const char *pattern = "(http[s]?://[\\w\\-\\.]+(:\\d+)?(/\\S*)?)";
  g_autoptr(GRegex) regex = g_regex_new(pattern, G_REGEX_CASELESS, 0, &error);
  if (error) return NULL;
  gchar *markup = g_regex_replace(regex, escaped_text, -1, 0, "<a href=\"\\0\">\\0</a>", 0, &error);
  if (error) return NULL;

  return markup;
}
