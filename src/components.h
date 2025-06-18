#pragma once

#include "glib-object.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include <adwaita.h>

#include <stdarg.h>
#include <stdlib.h>

// --- CHECK MENU ITEM --- //

static void __errands_menu_check_activate(void *data[2]) {
  bool active = !gtk_check_button_get_active(data[0]);
  gtk_check_button_set_active(data[0], active);
  void (*callback)(bool _active) = data[1];
  callback(active);
}

static inline GtkWidget *errands_menu_check_item(const char *label, bool active, void (*callback)(bool active)) {
  GtkWidget *check = gtk_check_button_new();
  gtk_check_button_set_active(GTK_CHECK_BUTTON(check), active);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_add_css_class(box, "menu-item");
  gtk_box_append(GTK_BOX(box), check);
  gtk_box_append(GTK_BOX(box), gtk_label_new(label));

  GtkGesture *ctrl = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(ctrl), 1);
  void **data = malloc(sizeof(void *) * 2);
  data[0] = check;
  data[1] = callback;
  g_signal_connect_swapped(ctrl, "released", G_CALLBACK(__errands_menu_check_activate), data);
  gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(ctrl));

  return box;
}

// --- RADIO MENU ITEM --- //

static void __errands_menu_radio_activate(void *data[3]) {
  GtkCheckButton *btn = data[0];
  const char *id = data[1];
  bool is_active = gtk_check_button_get_active(btn);
  if (is_active) return;
  gtk_check_button_set_active(btn, !is_active);
  void (*callback)(const char *active_id) = data[2];
  callback(id);
}

static inline GtkWidget *errands_menu_radio_item(const char *label, const char *id, bool active, GtkWidget *group,
                                                 void (*callback)(const char *active_id)) {
  GtkWidget *check = gtk_check_button_new();
  gtk_check_button_set_active(GTK_CHECK_BUTTON(check), active);
  if (group) {
    GtkCheckButton *group_check = g_object_get_data(G_OBJECT(group), "check");
    gtk_check_button_set_group(GTK_CHECK_BUTTON(check), group_check);
  }

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_add_css_class(box, "menu-item");
  g_object_set_data(G_OBJECT(box), "check", check);
  gtk_box_append(GTK_BOX(box), check);
  gtk_box_append(GTK_BOX(box), gtk_label_new(label));

  GtkGesture *ctrl = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(ctrl), 1);
  void **data = malloc(sizeof(void *) * 3);
  data[0] = check;
  data[1] = strdup(id);
  data[2] = callback;
  g_signal_connect_swapped(ctrl, "released", G_CALLBACK(__errands_menu_radio_activate), data);
  gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(ctrl));

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

static inline GtkWidget *errands_check_row_new(const char *title, const char *icon_name, GtkWidget *group,
                                               void (*cb)(GtkCheckButton *btn, void *data), void *cb_data) {
  GtkWidget *toggle = gtk_check_button_new();
  if (group)
    gtk_check_button_set_group(GTK_CHECK_BUTTON(toggle),
                               GTK_CHECK_BUTTON(g_object_get_data(G_OBJECT(group), "toggle")));

  GtkWidget *row = adw_action_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
  adw_action_row_add_suffix(ADW_ACTION_ROW(row), toggle);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), toggle);
  g_object_set_data(G_OBJECT(row), "toggle", toggle);
  if (icon_name) adw_action_row_add_prefix(ADW_ACTION_ROW(row), gtk_image_new_from_icon_name(icon_name));
  if (cb) g_signal_connect(toggle, "toggled", G_CALLBACK(cb), cb_data);

  return row;
}

// Box with children
static inline GtkWidget *errands_box_new(GtkOrientation orientation, size_t spacing, GtkWidget *first_child, ...) {
  GtkWidget *box = gtk_box_new(orientation, spacing);
  if (first_child != NULL) {
    gtk_box_append(GTK_BOX(box), first_child);
    va_list children;
    va_start(children, first_child);
    GtkWidget *child;
    while ((child = va_arg(children, GtkWidget *)) != NULL) gtk_box_append(GTK_BOX(box), child);
    va_end(children);
  }
  return box;
}

static inline GtkWidget *errands_toolbar_view_new(GtkWidget *child, GtkWidget *top_bar, GtkWidget *bottom_bar) {
  if (!child) return NULL;
  GtkWidget *tb = adw_toolbar_view_new();
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), child);
  if (top_bar) adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), top_bar);
  if (bottom_bar) adw_toolbar_view_add_bottom_bar(ADW_TOOLBAR_VIEW(tb), bottom_bar);
  return tb;
}
