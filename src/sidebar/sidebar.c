#include "sidebar.h"
#include "../components.h"
#include "../data.h"
#include "../state.h"
#include "../task-list.h"
#include "../utils.h"
#include "sidebar-all-row.h"

#include <glib/gi18n.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- DECLARATIONS --- //

static void on_errands_sidebar_filter_row_activated(GtkListBox *box,
                                                    GtkListBoxRow *row,
                                                    gpointer data);

// --- IMPLEMENTATIONS --- //

G_DEFINE_TYPE(ErrandsSidebar, errands_sidebar, ADW_TYPE_BIN)

static void errands_sidebar_class_init(ErrandsSidebarClass *class) {}

static void errands_sidebar_init(ErrandsSidebar *self) {
  LOG("Creating sidebar");

  // Toolbar View
  GtkWidget *tb = adw_toolbar_view_new();
  adw_bin_set_child(ADW_BIN(self), tb);

  // Headerbar
  GtkWidget *hb = adw_header_bar_new();
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb),
                                  adw_window_title_new("Errands", ""));
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);

  // Add list button
  GtkWidget *add_btn = gtk_button_new_from_icon_name("errands-add-symbolic");
  g_signal_connect(add_btn, "clicked", G_CALLBACK(errands_new_list_dialog_show),
                   NULL);
  adw_header_bar_pack_start(ADW_HEADER_BAR(hb), add_btn);
  errands_add_shortcut(add_btn, "<Control><Shift>N", "activate");

  // Menu button
  GtkWidget *menu_btn = gtk_menu_button_new();
  g_object_set(menu_btn, "icon-name", "open-menu-symbolic", NULL);
  adw_header_bar_pack_end(ADW_HEADER_BAR(hb), menu_btn);

  // Filter rows
  self->filters_box = gtk_list_box_new();
  gtk_widget_add_css_class(self->filters_box, "navigation-sidebar");
  g_signal_connect(self->filters_box, "row-activated",
                   G_CALLBACK(on_errands_sidebar_filter_row_activated), NULL);

  // All tasks row
  self->all_row = errands_sidebar_all_row_new();
  errands_sidebar_all_row_update_counter(self->all_row);
  gtk_list_box_append(GTK_LIST_BOX(self->filters_box),
                      GTK_WIDGET(self->all_row));

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
  self->task_lists_box = gtk_list_box_new();
  gtk_widget_add_css_class(self->task_lists_box, "navigation-sidebar");
  g_signal_connect(self->task_lists_box, "row-activated",
                   G_CALLBACK(on_errands_sidebar_task_list_row_activate), NULL);
  // Add rows
  for (int i = 0; i < state.tl_data->len; i++) {
    TaskListData *tld = state.tl_data->pdata[i];
    if (!tld->deleted)
      errands_sidebar_add_task_list(self, tld);
  }

  // Sidebar content box
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(box), self->filters_box);
  gtk_box_append(GTK_BOX(box), errands_separator_new(_("Task Lists")));
  gtk_box_append(GTK_BOX(box), self->task_lists_box);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl), box);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), scrl);

  // Select last opened page
  g_signal_connect(state.main_window, "realize",
                   G_CALLBACK(errands_sidebar_select_last_opened_page), NULL);
}

ErrandsSidebar *errands_sidebar_new() {
  return g_object_new(ERRANDS_TYPE_SIDEBAR, NULL);
}

ErrandsSidebarTaskListRow *errands_sidebar_add_task_list(ErrandsSidebar *sb,
                                                         TaskListData *data) {
  ErrandsSidebarTaskListRow *row = errands_sidebar_task_list_row_new(data);
  gtk_list_box_append(GTK_LIST_BOX(sb->task_lists_box), GTK_WIDGET(row));
  return row;
}

void errands_sidebar_select_last_opened_page() {
  // TODO: get page from settings
  g_signal_emit_by_name(state.sidebar->all_row, "activate", NULL);
}

// --- SIGNAL HANDLERS --- //

static void on_errands_sidebar_filter_row_activated(GtkListBox *box,
                                                    GtkListBoxRow *row,
                                                    gpointer data) {
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.sidebar->task_lists_box));
  if (GTK_WIDGET(row) == GTK_WIDGET(state.sidebar->all_row)) {
    LOG("Switch to all tasks page");
    adw_view_stack_set_visible_child_name(
        ADW_VIEW_STACK(state.main_window->stack), "errands_task_list_page");
    state.task_list->data = NULL;
    errands_task_list_update_title();
    errands_task_list_filter_by_uid("");
    gtk_revealer_set_reveal_child(GTK_REVEALER(state.task_list->entry), false);
  }
}
