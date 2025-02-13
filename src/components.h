#pragma once

#include <adwaita.h>

#include <stdarg.h>
#include <stdlib.h>

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
  if (is_active)
    return;
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
