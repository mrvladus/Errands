#include "sidebar.h"
#include "adwaita.h"
#include "components.h"
#include "data.h"
#include "glib-object.h"
#include "sidebar-all-row.h"
#include "sidebar-task-list-row.h"
#include "state.h"
#include "task-list.h"

#include <gtk/gtk.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- GLOBAL STATE VARIABLES --- //

static AdwDialog *new_list_dialog;
static GtkWidget *new_list_dialog_entry;

static void on_entry_changed(AdwEntryRow *entry) {
  adw_alert_dialog_set_response_enabled(
      ADW_ALERT_DIALOG(new_list_dialog), "create",
      strcmp("", gtk_editable_get_text(GTK_EDITABLE(entry))));
}

static void on_response(AdwAlertDialog *dialog, gchar *response,
                        gpointer user_data) {
  if (!strcmp(response, "create")) {
    TaskListData *tld = errands_data_add_list(
        gtk_editable_get_text(GTK_EDITABLE(new_list_dialog_entry)));
    errands_sidebar_add_task_list(tld);
  }
}

static void new_list_dialog_build() {
  // Box
  GtkWidget *box = gtk_list_box_new();
  g_object_set(box, "selection-mode", GTK_SELECTION_NONE, NULL);
  gtk_widget_add_css_class(box, "boxed-list");

  // Entry
  new_list_dialog_entry = adw_entry_row_new();
  g_object_set(new_list_dialog_entry, "title", "List Name", NULL);
  g_signal_connect(new_list_dialog_entry, "changed",
                   G_CALLBACK(on_entry_changed), NULL);
  gtk_list_box_append(GTK_LIST_BOX(box), new_list_dialog_entry);

  new_list_dialog = g_object_ref_sink(adw_alert_dialog_new("Add List", ""));
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(new_list_dialog), "cancel",
                                "Cancel");
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(new_list_dialog), "create",
                                "Create");
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(new_list_dialog),
                                           "create", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(new_list_dialog),
                                        "cancel");
  adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(new_list_dialog),
                                      "cancel");
  adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(new_list_dialog), box);
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(new_list_dialog),
                                        "create", false);
  g_signal_connect(new_list_dialog, "response", G_CALLBACK(on_response), NULL);
}

static void new_list_dialog_show(GtkButton *btn) {
  adw_dialog_present(new_list_dialog, state.main_window);
  gtk_editable_set_text(GTK_EDITABLE(new_list_dialog_entry), "");
}

static void on_errands_sidebar_row_activated(GtkListBox *box,
                                             GtkListBoxRow *row,
                                             gpointer data) {
  gtk_list_box_unselect_all(GTK_LIST_BOX(state.task_lists_list_box));
  if (GTK_WIDGET(row) == state.all_row) {
    LOG("Switch to all tasks page");
    adw_view_stack_set_visible_child_name(ADW_VIEW_STACK(state.stack),
                                          "errands_task_list_page");
    errands_task_list_filter_by_uid("");
    gtk_widget_set_visible(state.task_list_entry, false);
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
  new_list_dialog_build();
  GtkWidget *sb_hb_add_btn =
      gtk_button_new_from_icon_name("errands-add-symbolic");
  g_signal_connect(sb_hb_add_btn, "clicked", G_CALLBACK(new_list_dialog_show),
                   NULL);
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
  gtk_list_box_append(GTK_LIST_BOX(state.filters_list_box), state.all_row);

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

void errands_sidebar_add_task_list(TaskListData *data) {
  GtkWidget *row = errands_sidebar_task_list_row_new(data);
  gtk_list_box_append(GTK_LIST_BOX(state.task_lists_list_box), row);
}
