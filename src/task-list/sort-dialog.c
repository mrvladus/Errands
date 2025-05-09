#include "../settings.h"
#include "../state.h"
#include "adwaita.h"
#include "glib-object.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "task-list.h"

#include <glib/gi18n.h>

static void on_show_completed_toggle(AdwSwitchRow *row);

G_DEFINE_TYPE(ErrandsSortDialog, errands_sort_dialog, ADW_TYPE_DIALOG)

static void errands_sort_dialog_class_init(ErrandsSortDialogClass *class) {}

static void errands_sort_dialog_init(ErrandsSortDialog *self) {
  adw_dialog_set_content_width(ADW_DIALOG(self), 360);

  GtkWidget *list_box = g_object_new(GTK_TYPE_LIST_BOX, "selection-mode", GTK_SELECTION_NONE, "margin-start", 12,
                                     "margin-end", 12, "margin-bottom", 18, "margin-top", 12, NULL);
  gtk_widget_add_css_class(list_box, "boxed-list");

  GtkWidget *show_completed = g_object_new(ADW_TYPE_SWITCH_ROW, "title", _("Show Completed"), "active",
                                           errands_settings_get("show_completed", SETTING_TYPE_BOOL).b, NULL);
  g_signal_connect(show_completed, "notify::active", G_CALLBACK(on_show_completed_toggle), NULL);
  gtk_list_box_append(GTK_LIST_BOX(list_box), show_completed);

  // Toolbar View
  GtkWidget *tb = g_object_new(ADW_TYPE_TOOLBAR_VIEW, "content", list_box, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb),
                               g_object_new(ADW_TYPE_HEADER_BAR, "title-widget",
                                            g_object_new(ADW_TYPE_WINDOW_TITLE, "title", _("Filter and Sort"), NULL),
                                            NULL));
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsSortDialog *errands_sort_dialog_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_SORT_DIALOG, NULL));
}

void errands_sort_dialog_show() {
  if (!state.sort_dialog) state.sort_dialog = errands_sort_dialog_dialog_new();
  adw_dialog_present(ADW_DIALOG(state.sort_dialog), GTK_WIDGET(state.main_window));
}

static void on_show_completed_toggle(AdwSwitchRow *row) {
  bool completed = adw_switch_row_get_active(row);
  errands_settings_set("show_completed", SETTING_TYPE_BOOL, &completed);
}
