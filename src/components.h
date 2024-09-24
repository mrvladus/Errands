#ifndef ERRANDS_COMPONENTS_H
#define ERRANDS_COMPONENTS_H

#include "state.h"
#include <adwaita.h>

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

static inline void errands_datetime_window_build() {
  // Header Bar
  GtkWidget *hb = adw_header_bar_new();
  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "child", hb, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  // Window
  AdwDialog *win = adw_dialog_new();
  g_object_set(win, "child", tb, NULL);
  state.notes_window = win;
}

static inline void errands_tags_window_build() {
  // Header Bar
  GtkWidget *hb = adw_header_bar_new();
  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "child", hb, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  // Window
  AdwDialog *win = adw_dialog_new();
  g_object_set(win, "child", tb, NULL);
  state.notes_window = win;
}

static inline void errands_priority_window_build() {
  // Header Bar
  GtkWidget *hb = adw_header_bar_new();
  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "child", hb, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  // Window
  AdwDialog *win = adw_dialog_new();
  g_object_set(win, "child", tb, NULL);
  state.notes_window = win;
}

static inline void errands_attachments_window_build() {
  // Header Bar
  GtkWidget *hb = adw_header_bar_new();
  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "child", hb, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  // Window
  AdwDialog *win = adw_dialog_new();
  g_object_set(win, "child", tb, NULL);
  state.notes_window = win;
}

#endif // ERRANDS_COMPONENTS_H
