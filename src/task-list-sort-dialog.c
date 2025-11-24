#include "settings.h"
#include "state.h"
#include "task-list.h"

static void on_show_completed_toggle_cb(AdwSwitchRow *row);
static void on_created_toggle_cb(GtkCheckButton *btn);
static void on_start_toggle_cb(GtkCheckButton *btn);
static void on_due_toggle_cb(GtkCheckButton *btn);
static void on_priority_toggle_cb(GtkCheckButton *btn);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListSortDialog {
  AdwDialog parent_instance;
  GtkWidget *completed_toggle_row;
  GtkWidget *creation_date_toggle_btn;
  GtkWidget *start_date_toggle_btn;
  GtkWidget *due_date_toggle_btn;
  GtkWidget *priority_toggle_btn;
  bool block_signals;
};

G_DEFINE_TYPE(ErrandsTaskListSortDialog, errands_task_list_sort_dialog, ADW_TYPE_DIALOG)

static void errands_task_list_sort_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_LIST_SORT_DIALOG);
  G_OBJECT_CLASS(errands_task_list_sort_dialog_parent_class)->dispose(gobject);
}

static void errands_task_list_sort_dialog_class_init(ErrandsTaskListSortDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_list_sort_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-list-sort-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, completed_toggle_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, creation_date_toggle_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, start_date_toggle_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, due_date_toggle_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, priority_toggle_btn);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_show_completed_toggle_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_created_toggle_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_start_toggle_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_due_toggle_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_priority_toggle_cb);
}

static void errands_task_list_sort_dialog_init(ErrandsTaskListSortDialog *self) {
  gtk_widget_init_template(GTK_WIDGET(self));
}

ErrandsTaskListSortDialog *errands_task_list_sort_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_LIST_SORT_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_list_sort_dialog_show() {
  if (!state.main_window->task_list->sort_dialog)
    state.main_window->task_list->sort_dialog = errands_task_list_sort_dialog_new();
  ErrandsTaskListSortDialog *dialog = state.main_window->task_list->sort_dialog;
  dialog->block_signals = true;
  adw_switch_row_set_active(ADW_SWITCH_ROW(dialog->completed_toggle_row),
                            errands_settings_get(SETTING_SHOW_COMPLETED).b);
  ErrandsSortType sort_by = errands_settings_get(SETTING_SORT_BY).i;
  gtk_check_button_set_active(GTK_CHECK_BUTTON(dialog->creation_date_toggle_btn), sort_by == SORT_TYPE_CREATION_DATE);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(dialog->start_date_toggle_btn), sort_by == SORT_TYPE_START_DATE);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(dialog->due_date_toggle_btn), sort_by == SORT_TYPE_DUE_DATE);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(dialog->priority_toggle_btn), sort_by == SORT_TYPE_PRIORITY);
  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(state.main_window));
  dialog->block_signals = false;
}

// ---------- CALLBACKS ---------- //

static void on_show_completed_toggle_cb(AdwSwitchRow *row) {
  if (state.main_window->task_list->sort_dialog->block_signals) return;
  bool show_completed = adw_switch_row_get_active(row);
  errands_settings_set(SETTING_SHOW_COMPLETED, &show_completed);
  // gtk_filter_changed(GTK_FILTER(state.main_window->task_list->master_filter),
  //                    show_completed ? GTK_FILTER_CHANGE_LESS_STRICT : GTK_FILTER_CHANGE_MORE_STRICT);
}

static void set_sort_by(GtkCheckButton *btn, size_t sort_by) {
  if (state.main_window->task_list->sort_dialog->block_signals) return;
  size_t sort_by_current = errands_settings_get(SETTING_SORT_BY).i;
  if (!gtk_check_button_get_active(btn) || sort_by_current == sort_by) return;
  errands_settings_set(SETTING_SORT_BY, &sort_by);
  // gtk_sorter_changed(GTK_SORTER(state.main_window->task_list->master_sorter), GTK_SORTER_CHANGE_MORE_STRICT);
  // gtk_list_view_scroll_to(GTK_LIST_VIEW(state.main_window->task_list->task_list), 0, GTK_LIST_SCROLL_FOCUS, NULL);
}

static void on_created_toggle_cb(GtkCheckButton *btn) { set_sort_by(btn, SORT_TYPE_CREATION_DATE); }

static void on_start_toggle_cb(GtkCheckButton *btn) { set_sort_by(btn, SORT_TYPE_START_DATE); }

static void on_due_toggle_cb(GtkCheckButton *btn) { set_sort_by(btn, SORT_TYPE_DUE_DATE); }

static void on_priority_toggle_cb(GtkCheckButton *btn) { set_sort_by(btn, SORT_TYPE_PRIORITY); }
