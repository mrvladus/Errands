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

#endif // ERRANDS_COMPONENTS_H
