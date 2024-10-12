#pragma once

#include <adwaita.h>

#include <stdarg.h>

static inline GtkWidget *errands_separator_new(const char *title) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  g_object_set(box, "margin-start", 12, "margin-end", 12, NULL);
  gtk_widget_add_css_class(box, "dim-label");

  GtkWidget *left_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_set(left_sep, "hexpand", true, "valign", GTK_ALIGN_CENTER, NULL);
  gtk_box_append(GTK_BOX(box), left_sep);

  GtkWidget *label = gtk_label_new(title);
  gtk_widget_add_css_class(label, "caption");
  gtk_box_append(GTK_BOX(box), label);

  GtkWidget *right_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_set(right_sep, "hexpand", true, "valign", GTK_ALIGN_CENTER, NULL);
  gtk_box_append(GTK_BOX(box), right_sep);

  return box;
}

static inline GMenu *errands_menu_new(int n_items, ...) {
  va_list args;
  va_start(args, n_items);
  GMenu *menu = g_menu_new();
  for (int i = 0; i < n_items; i++) {
    const char *name = va_arg(args, const char *);
    const char *action = va_arg(args, const char *);
    g_menu_append(menu, name, action);
  }
  va_end(args);
  return menu;
}

static inline GSimpleActionGroup *errands_action_group_new(int n_items, ...) {
  va_list args;
  va_start(args, n_items);
  GSimpleActionGroup *ag = g_simple_action_group_new();
  for (int i = 0; i < n_items; i++) {
    const char *name = va_arg(args, const char *);
    void *cb = va_arg(args, void *);
    void *arg = va_arg(args, void *);
    GSimpleAction *action = g_simple_action_new(name, NULL);
    g_signal_connect(action, "activate", G_CALLBACK(cb), arg);
    g_action_map_add_action(G_ACTION_MAP(ag), G_ACTION(action));
  }
  va_end(args);
  return ag;
}
