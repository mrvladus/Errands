#include "settings.h"
#include "state.h"
#include "task-list.h"

static void on_show_completed_toggle_cb(AdwSwitchRow *row);
static void on_show_cancelled_toggle_cb(AdwSwitchRow *row);
static void on_created_toggle_cb(GtkCheckButton *btn);
static void on_start_toggle_cb(GtkCheckButton *btn);
static void on_due_toggle_cb(GtkCheckButton *btn);
static void on_priority_toggle_cb(GtkCheckButton *btn);
static void on_sort_order_changed_cb(ErrandsTaskListSortDialog *self);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskListSortDialog {
  AdwDialog parent_instance;
  GtkWidget *completed_toggle_row;
  GtkWidget *cancelled_toggle_row;
  AdwToggleGroup *sort_order;
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
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, cancelled_toggle_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, sort_order);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, creation_date_toggle_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, start_date_toggle_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, due_date_toggle_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskListSortDialog, priority_toggle_btn);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_show_completed_toggle_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_show_cancelled_toggle_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_sort_order_changed_cb);
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
  adw_switch_row_set_active(ADW_SWITCH_ROW(dialog->cancelled_toggle_row),
                            errands_settings_get(SETTING_SHOW_CANCELLED).b);
  adw_toggle_group_set_active(dialog->sort_order, errands_settings_get(SETTING_SORT_ORDER).i);
  ErrandsSettingSortType sort_by = errands_settings_get(SETTING_SORT_BY).i;
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
  errands_task_list_filter(state.main_window->task_list, GTK_FILTER_CHANGE_DIFFERENT);
}

static void on_show_cancelled_toggle_cb(AdwSwitchRow *row) {
  if (state.main_window->task_list->sort_dialog->block_signals) return;
  bool show_cancelled = adw_switch_row_get_active(row);
  errands_settings_set(SETTING_SHOW_CANCELLED, &show_cancelled);
  errands_task_list_filter(state.main_window->task_list,
                           show_cancelled ? GTK_FILTER_CHANGE_LESS_STRICT : GTK_FILTER_CHANGE_MORE_STRICT);
}

static void set_sort_by(GtkCheckButton *btn, size_t sort_by) {
  if (state.main_window->task_list->sort_dialog->block_signals) return;
  size_t sort_by_current = errands_settings_get(SETTING_SORT_BY).i;
  if (!gtk_check_button_get_active(btn) || sort_by_current == sort_by) return;
  errands_settings_set(SETTING_SORT_BY, &sort_by);
  errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_DIFFERENT);
}

static void on_created_toggle_cb(GtkCheckButton *btn) { set_sort_by(btn, SORT_TYPE_CREATION_DATE); }

static void on_start_toggle_cb(GtkCheckButton *btn) { set_sort_by(btn, SORT_TYPE_START_DATE); }

static void on_due_toggle_cb(GtkCheckButton *btn) { set_sort_by(btn, SORT_TYPE_DUE_DATE); }

static void on_priority_toggle_cb(GtkCheckButton *btn) { set_sort_by(btn, SORT_TYPE_PRIORITY); }

static void on_sort_order_changed_cb(ErrandsTaskListSortDialog *self) {
  if (state.main_window->task_list->sort_dialog->block_signals) return;
  size_t sort_by_current = errands_settings_get(SETTING_SORT_ORDER).i;
  size_t new = adw_toggle_group_get_active(self->sort_order);
  if (new == sort_by_current) return;
  errands_settings_set(SETTING_SORT_ORDER, &new);
  errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_INVERTED);
}
