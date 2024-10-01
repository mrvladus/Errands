#include "sidebar.h"
#include "components.h"
#include "data.h"
#include "gtk/gtkrevealer.h"
#include "new-list-dialog.h"
#include "sidebar-all-row.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

#include <gtk/gtk.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_errands_sidebar_row_activated(GtkListBox *box,
                                             GtkListBoxRow *row,
                                             gpointer data) {
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.task_lists_list_box));
  if (GTK_WIDGET(row) == GTK_WIDGET(state.all_row)) {
    LOG("Switch to all tasks page");
    adw_view_stack_set_visible_child_name(ADW_VIEW_STACK(state.stack),
                                          "errands_task_list_page");
    state.task_list->data = NULL;
    errands_task_list_update_title();
    errands_task_list_filter_by_uid("");
    gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), false);
  }
}

void errands_sidebar_build() {
  // Toolbar View
  GtkWidget *tb = adw_toolbar_view_new();

  // Headerbar
  GtkWidget *sb_hb = adw_header_bar_new();
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(sb_hb),
                                  adw_window_title_new("Errands", ""));
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), sb_hb);

  // Add list button
  errands_new_list_dialog_build();
  GtkWidget *sb_hb_add_btn =
      gtk_button_new_from_icon_name("errands-add-symbolic");
  g_signal_connect(sb_hb_add_btn, "clicked",
                   G_CALLBACK(errands_new_list_dialog_show), NULL);
  adw_header_bar_pack_start(ADW_HEADER_BAR(sb_hb), sb_hb_add_btn);

  // Menu button
  GtkWidget *sb_hb_menu_btn = gtk_menu_button_new();
  g_object_set(sb_hb_menu_btn, "icon-name", "open-menu-symbolic", NULL);
  adw_header_bar_pack_end(ADW_HEADER_BAR(sb_hb), sb_hb_menu_btn);

  // Filter rows
  state.filters_list_box = gtk_list_box_new();
  gtk_widget_add_css_class(state.filters_list_box, "navigation-sidebar");
  g_signal_connect(state.filters_list_box, "row-activated",
                   G_CALLBACK(on_errands_sidebar_row_activated), NULL);

  // All tasks row
  state.all_row = errands_sidebar_all_row_new();
  errands_sidebar_all_row_update_counter();
  gtk_list_box_append(GTK_LIST_BOX(state.filters_list_box),
                      GTK_WIDGET(state.all_row));

  // // Today row
  // GtkWidget *today_row = errands_sidebar_row_new(
  //     "errands_today_page", "errands-today-symbolic", "Today", NULL);
  // gtk_list_box_append(GTK_LIST_BOX(state.filters_list_box), today_row);
  // // Today row
  // GtkWidget *tags_row = errands_sidebar_row_new(
  //     "errands_tags_page", "errands-tag-symbolic", "Tags", NULL);
  // gtk_list_box_append(GTK_LIST_BOX(state.filters_list_box), tags_row);
  // // Trash row
  // GtkWidget *trash_row = errands_sidebar_row_new(
  //     "errands_trash_page", "errands-trash-symbolic", "Trash", NULL);
  // gtk_list_box_append(GTK_LIST_BOX(state.filters_list_box), trash_row);

  // Task Lists rows
  state.task_lists_list_box = gtk_list_box_new();
  gtk_widget_add_css_class(state.task_lists_list_box, "navigation-sidebar");
  g_signal_connect(state.task_lists_list_box, "row-activated",
                   G_CALLBACK(on_errands_sidebar_task_list_row_activate), NULL);
  // Add rows
  for (int i = 0; i < state.tl_data->len; i++) {
    TaskListData *tld = state.tl_data->pdata[i];
    if (!tld->deleted)
      errands_sidebar_add_task_list(tld);
  }

  // Sidebar content box
  GtkWidget *sb_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(sb_box), state.filters_list_box);
  gtk_box_append(GTK_BOX(sb_box), errands_separator_new("Task Lists"));
  gtk_box_append(GTK_BOX(sb_box), state.task_lists_list_box);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl), sb_box);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), scrl);
  state.sidebar = tb;

  // Select last opened page
  adw_view_stack_set_visible_child_name(ADW_VIEW_STACK(state.stack),
                                        "errands_task_list_page");
}

ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(TaskListData *data) {
  ErrandsSidebarTaskListRow *row = errands_sidebar_task_list_row_new(data);
  gtk_list_box_append(GTK_LIST_BOX(state.task_lists_list_box), GTK_WIDGET(row));
  return row;
}
