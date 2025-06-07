#include "../components.h"
#include "../settings.h"
#include "../state.h"
#include "../utils.h"
#include "task-list.h"

#include <glib/gi18n.h>
#include <stdbool.h>

static void on_errands_sort_dialog_close_cb(ErrandsSortDialog *self);
static void on_show_completed_toggle(AdwSwitchRow *row);
static void on_created_toggle(GtkCheckButton *btn, void *data);
static void on_due_toggle(GtkCheckButton *btn, void *data);
static void on_priority_toggle(GtkCheckButton *btn, void *data);

G_DEFINE_TYPE(ErrandsSortDialog, errands_sort_dialog, ADW_TYPE_DIALOG)

static void errands_sort_dialog_class_init(ErrandsSortDialogClass *class) {}

static void errands_sort_dialog_init(ErrandsSortDialog *self) {
  adw_dialog_set_content_width(ADW_DIALOG(self), 360);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_sort_dialog_close_cb), NULL);

  GtkWidget *page = adw_preferences_page_new();

  // Filter
  GtkWidget *filter_group = g_object_new(ADW_TYPE_PREFERENCES_GROUP, "title", _("Filter"), NULL);
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(filter_group));

  self->show_completed_row = g_object_new(ADW_TYPE_SWITCH_ROW, "title", _("Completed Tasks"), "active",
                                          errands_settings_get("show_completed", SETTING_TYPE_BOOL).b, NULL);
  g_signal_connect(self->show_completed_row, "notify::active", G_CALLBACK(on_show_completed_toggle), NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(filter_group), self->show_completed_row);

  // Sort
  GtkWidget *sort_group = g_object_new(ADW_TYPE_PREFERENCES_GROUP, "title", _("Sort By"), NULL);
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(sort_group));

  // Creation Date
  self->created_row = errands_check_row_new(_("Creation Date"), NULL, NULL, on_created_toggle, NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(sort_group), self->created_row);
  // Due Date
  self->due_row = errands_check_row_new(_("Due Date"), NULL, self->created_row, on_due_toggle, NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(sort_group), self->due_row);
  // Priority
  self->priority_row = errands_check_row_new(_("Priority"), NULL, self->created_row, on_priority_toggle, NULL);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(sort_group), self->priority_row);

  // Toolbar View
  GtkWidget *tb = g_object_new(ADW_TYPE_TOOLBAR_VIEW, "content", page, NULL);
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
  state.sort_dialog->block_signals = true;
  state.sort_dialog->sort_changed = false;

  bool show_completed = errands_settings_get("show_completed", SETTING_TYPE_STRING).b;
  adw_switch_row_set_active(ADW_SWITCH_ROW(state.sort_dialog->show_completed_row), show_completed);

  const char *sort_by = errands_settings_get("sort_by", SETTING_TYPE_STRING).s;
  if (g_str_equal(sort_by, "created")) adw_action_row_activate(ADW_ACTION_ROW(state.sort_dialog->created_row));
  else if (g_str_equal(sort_by, "due")) adw_action_row_activate(ADW_ACTION_ROW(state.sort_dialog->due_row));
  else if (g_str_equal(sort_by, "priority")) adw_action_row_activate(ADW_ACTION_ROW(state.sort_dialog->priority_row));

  adw_dialog_present(ADW_DIALOG(state.sort_dialog), GTK_WIDGET(state.main_window));
  state.sort_dialog->block_signals = false;
}

static void on_errands_sort_dialog_close_cb(ErrandsSortDialog *self) {
  if (!state.sort_dialog->sort_changed) return;
  errands_task_list_sort_recursive(state.task_list->task_list);
  state.sort_dialog->sort_changed = false;
}

static void on_show_completed_toggle(AdwSwitchRow *row) {
  if (state.sort_dialog->block_signals) return;
  errands_settings_set_bool("show_completed", adw_switch_row_get_active(row));
}

static void set_sort_by(GtkCheckButton *btn, const char *sort_by) {
  if (state.sort_dialog->block_signals) return;
  bool active = gtk_check_button_get_active(btn);
  const char *sort_by_current = errands_settings_get("sort_by", SETTING_TYPE_STRING).s;
  if (!active || g_str_equal(sort_by_current, sort_by)) return;
  LOG("Sort Window: Set sort by '%s'", sort_by);
  errands_settings_set_string("sort_by", sort_by);
  state.sort_dialog->sort_changed = true;
}

static void on_created_toggle(GtkCheckButton *btn, void *data) { set_sort_by(btn, "created"); }
static void on_due_toggle(GtkCheckButton *btn, void *data) { set_sort_by(btn, "due"); }
static void on_priority_toggle(GtkCheckButton *btn, void *data) { set_sort_by(btn, "priority"); }
