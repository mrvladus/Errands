#include "tags-page.h"
#include "gtk/gtk.h"
#include "state.h"
#include "utils.h"

#include <adwaita.h>

GtkWidget *errands_tag_new(char *text) {
  GtkWidget *row = adw_action_row_new();
  g_object_set(row, "title", text, "activatable", false, NULL);
  return row;
}

void errands_tags_page_build() {
  LOG("Creating tags page");
  // Header Bar
  GtkWidget *hb = adw_header_bar_new();
  // Entry
  GtkWidget *tags_entry = adw_entry_row_new();
  g_object_set(tags_entry, "title", "Add Tag", "activatable", false, NULL);
  gtk_widget_add_css_class(tags_entry, "card");
  GtkWidget *entry_clamp = adw_clamp_new();
  g_object_set(entry_clamp, "tightening-threshold", 300, "maximum-size", 1000,
               "margin-start", 12, "margin-end", 12, NULL);
  adw_clamp_set_child(ADW_CLAMP(entry_clamp), tags_entry);
  // Tags Box
  GtkWidget *tbox = gtk_list_box_new();
  g_object_set(tbox, "selection-mode", GTK_SELECTION_NONE, "margin-bottom", 18,
               "margin-top", 6, NULL);
  gtk_widget_add_css_class(tbox, "boxed-list");
  // Load Tags
  LOG("Creating tags");
  for (int i = 0; i < state.tags_data->len; i++) {
    char *text = state.tags_data->pdata[i];
    gtk_list_box_append(GTK_LIST_BOX(tbox), errands_tag_new(text));
  }
  // Clamp
  GtkWidget *tbox_clamp = adw_clamp_new();
  g_object_set(tbox_clamp, "tightening-threshold", 300, "maximum-size", 1000,
               "margin-start", 12, "margin-end", 12, NULL);
  adw_clamp_set_child(ADW_CLAMP(tbox_clamp), tbox);
  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  g_object_set(scrl, "propagate-natural-height", true,
               "propagate-natural-width", true, NULL);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl), tbox_clamp);
  // VBox
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_append(GTK_BOX(vbox), entry_clamp);
  gtk_box_append(GTK_BOX(vbox), scrl);
  // Toolbar View
  GtkWidget *tb = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), vbox);
  // Set data
  g_object_set_data(G_OBJECT(tb), "tags_list", tbox);
  state.tags_page = tb;
  // Create task list page in the view stack
  adw_view_stack_add_titled(ADW_VIEW_STACK(state.stack), tb,
                            "errands_tags_page", "Tags");
}
