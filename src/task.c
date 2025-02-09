#include "task.h"
#include "components.h"
#include "data.h"
#include "glib.h"
#include "settings.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <stdbool.h>
#include <stddef.h>

// ---------- SIGNALS ---------- //

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y, GtkPopover *popover);
static void on_action_rename(GSimpleAction *action, GVariant *param, ErrandsTask *task);
static void on_action_trash(GSimpleAction *action, GVariant *param, ErrandsTask *task);
static void on_errands_task_complete_btn_toggle(GtkCheckButton *btn, ErrandsTask *task);
static void on_errands_task_toolbar_btn_toggle(GtkToggleButton *btn, ErrandsTask *task);
static void on_errands_task_expand_click(GtkGestureClick *self, gint n_press, gdouble x, gdouble y, ErrandsTask *task);
static void on_errands_task_sub_task_added(GtkEntry *entry, ErrandsTask *task);
static void on_errands_task_edited(AdwEntryRow *entry, ErrandsTask *task);
static void on_errands_task_edit_cancelled(GtkButton *btn, ErrandsTask *task);

static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y, ErrandsTask *task);
static void on_drag_begin(GtkDragSource *source, GdkDrag *drag, ErrandsTask *task);
static void on_drag_end(GtkDragSource *self, GdkDrag *drag, gboolean delete_data, ErrandsTask *task);
static gboolean on_drag_cancel(GtkDragSource *self, GdkDrag *drag, GdkDragCancelReason *reason, ErrandsTask *task);
static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTask *task);
static gboolean on_top_area_drop(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTask *task);

// ---------- TASK ---------- //

G_DEFINE_TYPE(ErrandsTask, errands_task, ADW_TYPE_BIN)

static void errands_task_class_init(ErrandsTaskClass *class) {}

static void errands_task_init(ErrandsTask *self) {
  // Revealer
  self->revealer = gtk_revealer_new();
  adw_bin_set_child(ADW_BIN(self), self->revealer);

  // Task main box
  GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_revealer_set_child(GTK_REVEALER(self->revealer), main_box);

  // Top drop area
  GtkWidget *top_drop_area = gtk_image_new_from_icon_name("errands-add-symbolic");
  gtk_widget_add_css_class(top_drop_area, "task-top-drop-area");
  GtkDropTarget *top_drop_area_target = gtk_drop_target_new(G_TYPE_OBJECT, GDK_ACTION_MOVE);
  g_signal_connect(top_drop_area_target, "drop", G_CALLBACK(on_top_area_drop), self);
  gtk_widget_add_controller(GTK_WIDGET(top_drop_area), GTK_EVENT_CONTROLLER(top_drop_area_target));
  g_object_set(top_drop_area, "margin-start", 12, "margin-end", 12, NULL);
  gtk_box_append(GTK_BOX(main_box), top_drop_area);

  // Card
  self->card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_set(self->card, "margin-top", 6, "margin-bottom", 6, "margin-start", 12, "margin-end", 12, NULL);
  gtk_widget_add_css_class(self->card, "card");
  gtk_box_append(GTK_BOX(main_box), self->card);

  // Title list box
  GtkWidget *title_lb = gtk_list_box_new();
  g_object_set(title_lb, "selection-mode", GTK_SELECTION_NONE, NULL);
  gtk_widget_add_css_class(title_lb, "task-title-row");
  gtk_box_append(GTK_BOX(self->card), title_lb);

  // Title row
  self->title_row = adw_action_row_new();
  g_object_set(self->title_row, "hexpand", true, NULL);
  gtk_widget_add_css_class(self->title_row, "task-title-row");
  gtk_widget_set_cursor_from_name(self->title_row, "pointer");
  self->click_ctrl = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(self->click_ctrl), 1);
  gtk_widget_add_controller(self->title_row, GTK_EVENT_CONTROLLER(self->click_ctrl));
  gtk_list_box_append(GTK_LIST_BOX(title_lb), self->title_row);

  // Complete toggle
  self->complete_btn = gtk_check_button_new();
  g_object_set(self->complete_btn, "valign", GTK_ALIGN_CENTER, "active", false, NULL);
  gtk_widget_add_css_class(self->complete_btn, "selection-mode");
  adw_action_row_add_prefix(ADW_ACTION_ROW(self->title_row), self->complete_btn);

  // Toolbar toggle
  self->toolbar_btn = gtk_toggle_button_new();
  g_object_set(self->toolbar_btn, "icon-name", "errands-toolbar-symbolic", "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(self->toolbar_btn, "flat");
  gtk_widget_add_css_class(self->toolbar_btn, "circular");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self->title_row), self->toolbar_btn);

  // Edit row
  self->edit_row = adw_entry_row_new();
  g_object_set(self->edit_row, "title", _("Edit"), "hexpand", true, "show-apply-button", true, NULL);
  g_object_bind_property(self->title_row, "visible", self->edit_row, "visible",
                         G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_widget_add_css_class(self->edit_row, "task-title-row");
  g_signal_connect(self->edit_row, "apply", G_CALLBACK(on_errands_task_edited), self);
  g_signal_connect(self->edit_row, "entry-activated", G_CALLBACK(on_errands_task_edited), self);
  GtkWidget *cancel_btn = gtk_button_new_from_icon_name("errands-cancel-symbolic");
  g_object_set(cancel_btn, "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(cancel_btn, "flat");
  gtk_widget_add_css_class(cancel_btn, "circular");
  g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_errands_task_edit_cancelled), self);
  adw_entry_row_add_suffix(ADW_ENTRY_ROW(self->edit_row), cancel_btn);
  gtk_list_box_append(GTK_LIST_BOX(title_lb), self->edit_row);

  // Tags box
  self->tags_box = gtk_flow_box_new();
  g_object_set(self->tags_box, "max-children-per-line", 1000, "margin-bottom", 6, "margin-start", 12, "margin-end", 12,
               "selection-mode", GTK_SELECTION_NONE, "row-spacing", 6, "column-spacing", 6, NULL);

  // Tags revealer
  self->tags_revealer = gtk_revealer_new();
  g_object_set(self->tags_revealer, "child", self->tags_box, "transition-duration", 100, NULL);
  gtk_box_append(GTK_BOX(self->card), self->tags_revealer);

  // Progress bar
  self->progress_bar = gtk_progress_bar_new();
  g_object_set(self->progress_bar, "margin-start", 12, "margin-end", 12, "margin-bottom", 6, NULL);
  gtk_widget_add_css_class(self->progress_bar, "osd");
  gtk_widget_add_css_class(self->progress_bar, "dim-label");

  // Progress bar revealer
  self->progress_revealer = gtk_revealer_new();
  g_object_set(self->progress_revealer, "child", self->progress_bar, "transition-duration", 100, NULL);
  gtk_box_append(GTK_BOX(self->card), self->progress_revealer);

  // Toolbar revealer
  self->toolbar_revealer = gtk_revealer_new();
  g_object_bind_property(self->toolbar_btn, "active", self->toolbar_revealer, "reveal-child", G_BINDING_SYNC_CREATE);
  gtk_box_append(GTK_BOX(self->card), self->toolbar_revealer);

  // Sub-tasks revealer
  self->sub_tasks_revealer = gtk_revealer_new();
  gtk_box_append(GTK_BOX(self->card), self->sub_tasks_revealer);

  // Sub-tasks vbox
  GtkWidget *sub_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_revealer_set_child(GTK_REVEALER(self->sub_tasks_revealer), sub_vbox);

  // Sub-task entry
  self->sub_entry = gtk_entry_new();
  g_object_set(self->sub_entry, "margin-top", 0, "margin-bottom", 3, "margin-start", 12, "margin-end", 12,
               "placeholder-text", _("Add Sub-Task"), NULL);
  gtk_box_append(GTK_BOX(sub_vbox), self->sub_entry);

  // Sub-tasks
  self->sub_tasks = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(self->sub_tasks, "sub-tasks");
  gtk_box_append(GTK_BOX(sub_vbox), self->sub_tasks);

  // Right-click menu
  g_autoptr(GMenu) menu = errands_menu_new(2, _("Edit"), "task.edit", _("Move to Trash"), "task.trash");

  // Menu popover
  GtkWidget *menu_popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
  g_object_set(menu_popover, "has-arrow", false, "halign", GTK_ALIGN_START, NULL);
  gtk_box_append(GTK_BOX(self->card), menu_popover);

  // Right-click controllers
  GtkGesture *ctrl = gtk_gesture_click_new();
  gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(ctrl), 3);
  g_signal_connect(ctrl, "released", G_CALLBACK(on_right_click), menu_popover);
  gtk_widget_add_controller(self->title_row, GTK_EVENT_CONTROLLER(ctrl));
  GtkGesture *touch_ctrl = gtk_gesture_long_press_new();
  g_signal_connect(touch_ctrl, "pressed", G_CALLBACK(on_right_click), menu_popover);
  gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(touch_ctrl), true);
  gtk_widget_add_controller(self->title_row, GTK_EVENT_CONTROLLER(touch_ctrl));

  // Actions
  g_autoptr(GSimpleActionGroup) ag =
      errands_action_group_new(2, "edit", on_action_rename, self, "trash", on_action_trash, self);
  gtk_widget_insert_action_group(GTK_WIDGET(self), "task", G_ACTION_GROUP(ag));

  // DND
  GtkDragSource *drag_source = gtk_drag_source_new();
  gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
  g_signal_connect(drag_source, "prepare", G_CALLBACK(on_drag_prepare), self);
  g_signal_connect(drag_source, "drag-begin", G_CALLBACK(on_drag_begin), self);
  g_signal_connect(drag_source, "drag-end", G_CALLBACK(on_drag_end), self);
  g_signal_connect(drag_source, "drag-cancel", G_CALLBACK(on_drag_cancel), self);
  gtk_widget_add_controller(GTK_WIDGET(self->title_row), GTK_EVENT_CONTROLLER(drag_source));

  GtkDropTarget *drop_target = gtk_drop_target_new(G_TYPE_OBJECT, GDK_ACTION_MOVE);
  g_signal_connect(drop_target, "drop", G_CALLBACK(on_drop), self);
  gtk_widget_add_controller(GTK_WIDGET(self->title_row), GTK_EVENT_CONTROLLER(drop_target));

  GtkEventController *drop_ctrl = gtk_drop_controller_motion_new();
  g_object_bind_property(drop_ctrl, "contains-pointer", top_drop_area, "visible", G_BINDING_SYNC_CREATE);
  gtk_widget_add_controller(GTK_WIDGET(self), drop_ctrl);
}

ErrandsTask *errands_task_new(TaskData *data) {
  LOG("Task '%s': Create", task_data_get_uid(data));

  ErrandsTask *task = g_object_new(ERRANDS_TYPE_TASK, NULL);
  task->data = data;

  // Setup
  gtk_revealer_set_reveal_child(
      GTK_REVEALER(task->revealer),
      !task_data_get_deleted(data) && !task_data_get_trash(data) &&
          (!task_data_get_completed(data) || errands_settings_get("show_completed", SETTING_TYPE_BOOL).b));
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(task->title_row), task_data_get_text(data));
  gtk_check_button_set_active(GTK_CHECK_BUTTON(task->complete_btn), task_data_get_completed(data));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(task->toolbar_btn), task_data_get_toolbar_shown(data));

  // Lazy load toolbar
  if (task_data_get_toolbar_shown(data)) {
    task->toolbar = errands_task_toolbar_new(task);
    gtk_revealer_set_child(GTK_REVEALER(task->toolbar_revealer), GTK_WIDGET(task->toolbar));
  }
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->sub_tasks_revealer), task_data_get_expanded(data));

  // Load sub-tasks
  const char *task_uid = task_data_get_uid(data);
  g_autoptr(GPtrArray) tasks = list_data_get_tasks(task_data_get_list(data));
  for (size_t i = 0; i < tasks->len; i++) {
    TaskData *td = tasks->pdata[i];
    const char *parent = task_data_get_parent(td);
    const bool deleted = task_data_get_deleted(td);
    if (!strcmp(parent, task_uid) && !deleted) {
      LOG("Task '%s': Add sub task '%s'", task_uid, task_data_get_uid(td));
      gtk_box_append(GTK_BOX(task->sub_tasks), GTK_WIDGET(errands_task_new(td)));
    }
  }

  errands_task_list_sort_by_completion(task->sub_tasks);

  errands_task_update_tags(task);
  errands_task_update_accent_color(task);
  errands_task_update_progress(task);

  // Connect signals
  g_signal_connect(task->click_ctrl, "released", G_CALLBACK(on_errands_task_expand_click), task);
  g_signal_connect(task->complete_btn, "toggled", G_CALLBACK(on_errands_task_complete_btn_toggle), task);
  g_signal_connect(task->toolbar_btn, "toggled", G_CALLBACK(on_errands_task_toolbar_btn_toggle), task);
  g_signal_connect(task->sub_entry, "activate", G_CALLBACK(on_errands_task_sub_task_added), task);

  return task;
}

void errands_task_update_accent_color(ErrandsTask *task) {
  const char *color = task_data_get_color(task->data);
  if (strcmp(color, "none")) {
    g_autofree gchar *accent_style = g_strdup_printf("task-%s", color);
    gtk_widget_set_css_classes(GTK_WIDGET(task->card), (const char *[]){"vertical", "card", accent_style, NULL});
  } else {
    gtk_widget_set_css_classes(GTK_WIDGET(task->card), (const char *[]){"vertical", "card", NULL});
  }
}

void errands_task_update_progress(ErrandsTask *task) {
  g_autoptr(GPtrArray) sub_tasks = get_children(task->sub_tasks);
  size_t total = 0;
  size_t completed = 0;
  for (size_t i = 0; i < sub_tasks->len; i++) {
    ErrandsTask *sub_task = sub_tasks->pdata[i];
    TaskData *td = sub_task->data;
    if (!task_data_get_deleted(td) && !task_data_get_trash(td)) {
      total++;
      if (task_data_get_completed(td))
        completed++;
    }
  }
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->progress_revealer), total > 0);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(task->progress_bar), total > 0 ? (float)completed / (float)total : 0);

  // Set sub-title
  if (total == 0)
    g_object_set(task->title_row, "subtitle", "", NULL);
  else {
    g_autofree gchar *subtitle = g_strdup_printf(_("Completed: %zu / %zu"), completed, total);
    g_object_set(task->title_row, "subtitle", subtitle, NULL);
  }
}

static void __append_sub_tasks(GPtrArray *arr, ErrandsTask *task) {
  g_autoptr(GPtrArray) sub_tasks = get_children(task->sub_tasks);
  for (size_t i = 0; i < sub_tasks->len; i++) {
    ErrandsTask *t = sub_tasks->pdata[i];
    g_ptr_array_add(arr, t);
    __append_sub_tasks(arr, t);
  }
}

GPtrArray *errands_task_get_sub_tasks(ErrandsTask *task) {
  GPtrArray *arr = g_ptr_array_new();
  __append_sub_tasks(arr, task);
  return arr;
}

// --- TAGS --- //

static void errands_task_tag_delete(GtkWidget *label) {
  ErrandsTask *task = (ErrandsTask *)gtk_widget_get_ancestor(label, ERRANDS_TYPE_TASK);
  task_data_remove_tag(task->data, gtk_label_get_label(GTK_LABEL(label)));
  errands_data_write_list(task_data_get_list(task->data));
  errands_task_update_tags(task);
}

static GtkWidget *errands_task_tag_new(const char *tag) {
  // Box
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(box, "tag");

  // Label
  GtkWidget *label = gtk_label_new(tag);
  g_object_set(label, "max-width-chars", 15, "halign", GTK_ALIGN_START, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_widget_add_css_class(box, "caption-heading");
  gtk_box_append(GTK_BOX(box), label);

  // Button
  GtkWidget *btn = gtk_button_new_from_icon_name("errands-close-symbolic");
  g_signal_connect_swapped(btn, "clicked", G_CALLBACK(errands_task_tag_delete), label);
  gtk_box_append(GTK_BOX(box), btn);

  return box;
}

void errands_task_update_tags(ErrandsTask *task) {
  gtk_flow_box_remove_all(GTK_FLOW_BOX(task->tags_box));
  g_auto(GStrv) tags = task_data_get_tags(task->data);
  const size_t len = tags ? g_strv_length(tags) : 0;
  for_range(i, 0, len) gtk_flow_box_append(GTK_FLOW_BOX(task->tags_box), errands_task_tag_new(tags[i]));
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->tags_revealer), len > 0);
}

GPtrArray *errands_task_get_parents(ErrandsTask *task) {
  GPtrArray *parents = g_ptr_array_new();
  ErrandsTask *parent =
      ERRANDS_TASK(gtk_widget_get_ancestor(gtk_widget_get_parent(GTK_WIDGET(task)), ERRANDS_TYPE_TASK));
  while (parent) {
    g_ptr_array_add(parents, parent);
    parent = ERRANDS_TASK(gtk_widget_get_ancestor(gtk_widget_get_parent(GTK_WIDGET(parent)), ERRANDS_TYPE_TASK));
  }
  return parents;
}

// --- SIGNALS HANDLERS --- //

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y, GtkPopover *popover) {
  gtk_popover_set_pointing_to(popover, &(GdkRectangle){.x = x, .y = y});
  gtk_popover_popup(popover);
}

static void on_action_rename(GSimpleAction *action, GVariant *param, ErrandsTask *task) {
  gtk_widget_set_visible(task->title_row, false);
  gtk_editable_set_text(GTK_EDITABLE(task->edit_row), task_data_get_text(task->data));
  gtk_widget_grab_focus(task->edit_row);
}

static void on_action_trash(GSimpleAction *action, GVariant *param, ErrandsTask *task) {
  task_data_set_trash(task->data, true);
  errands_data_write_list(task_data_get_list(task->data));
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->revealer), false);
  errands_task_list_update_title();
  errands_sidebar_all_row_update_counter(state.sidebar->all_row);
  errands_sidebar_task_list_row_update_counter(errands_sidebar_task_list_row_get(task_data_get_list_uid(task->data)));
}

static void on_errands_task_complete_btn_toggle(GtkCheckButton *btn, ErrandsTask *task) {
  LOG("Toggle completion '%s'", task_data_get_uid(task->data));
  task_data_set_completed(task->data, gtk_check_button_get_active(btn));
  char dt[17];
  get_date_time(dt);
  task_data_set_changed(task->data, dt);
  task_data_set_synced(task->data, false);
  errands_data_write_list(state.task_list->data);

  GtkWidget *task_list;
  if (!strcmp(task_data_get_parent(task->data), ""))
    task_list = state.task_list->task_list;
  else
    task_list = gtk_widget_get_parent(GTK_WIDGET(task));

  // Complete all sub-tasks if task is completed
  if (task_data_get_completed(task->data)) {
    g_autoptr(GPtrArray) sub_tasks = get_children(task->sub_tasks);
    for (size_t i = 0; i < sub_tasks->len; i++) {
      ErrandsTask *sub_task = sub_tasks->pdata[i];
      gtk_check_button_set_active(GTK_CHECK_BUTTON(sub_task->complete_btn), true);
    }
  }

  // Update parents
  g_autoptr(GPtrArray) parents = errands_task_get_parents(task);
  for (size_t i = 0; i < parents->len; i++) {
    ErrandsTask *parent = parents->pdata[i];
    errands_task_update_progress(parent);
    // Uncomplete parent task if task is un-completed
    if (!gtk_check_button_get_active(btn))
      gtk_check_button_set_active(GTK_CHECK_BUTTON(parent->complete_btn), false);
  }

  gtk_revealer_set_reveal_child(
      GTK_REVEALER(task->revealer),
      !task_data_get_deleted(task->data) && !task_data_get_trash(task->data) &&
          (!task_data_get_completed(task->data) || errands_settings_get("show_completed", SETTING_TYPE_BOOL).b));

  // Update task list
  errands_task_list_sort_by_completion(task_list);
  errands_task_list_update_title();
  errands_sidebar_all_row_update_counter(state.sidebar->all_row);
  errands_sidebar_task_list_row_update_counter(errands_sidebar_task_list_row_get(task_data_get_list_uid(task->data)));
}

static void on_errands_task_toolbar_btn_toggle(GtkToggleButton *btn, ErrandsTask *task) {
  // Lazy load toolbar
  LOG("Task %s: Toggle toolbar", task_data_get_uid(task->data));
  if (!task->toolbar) {
    task->toolbar = errands_task_toolbar_new(task);
    gtk_revealer_set_child(GTK_REVEALER(task->toolbar_revealer), GTK_WIDGET(task->toolbar));
  }
  task_data_set_toolbar_shown(task->data, gtk_toggle_button_get_active(btn));
  errands_data_write_list(task_data_get_list(task->data));
}

static void on_errands_task_expand_click(GtkGestureClick *self, gint n_press, gdouble x, gdouble y, ErrandsTask *task) {
  const bool expanded = task_data_get_expanded(task->data);
  task_data_set_expanded(task->data, !expanded);
  errands_data_write_list(task_data_get_list(task->data));
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->sub_tasks_revealer), !expanded);
}

static void on_errands_task_sub_task_added(GtkEntry *entry, ErrandsTask *task) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (!strcmp(text, ""))
    return;
  TaskData *new_td = list_data_create_task(state.task_list->data, (char *)text, task_data_get_list_uid(task->data),
                                           task_data_get_uid(task->data));
  errands_data_write_list(state.task_list->data);
  gtk_box_prepend(GTK_BOX(task->sub_tasks), GTK_WIDGET(errands_task_new(new_td)));
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  errands_task_list_sort(task->sub_tasks);
}

static void on_errands_task_edited(AdwEntryRow *entry, ErrandsTask *task) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  const char *current_text = task_data_get_text(task->data);
  if (!strcmp(text, current_text) || !strcmp("", current_text))
    return;
  task_data_set_text(task->data, text);
  errands_data_write_list(task_data_get_list(task->data));
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(task->title_row), text);
  gtk_widget_set_visible(task->title_row, true);
  // TODO: sync
}

static void on_errands_task_edit_cancelled(GtkButton *btn, ErrandsTask *task) {
  gtk_widget_set_visible(task->title_row, true);
}

// - DND - //

static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y, ErrandsTask *task) {
  g_object_set(task, "sensitive", false, NULL);
  GValue value = G_VALUE_INIT;
  g_value_init(&value, G_TYPE_OBJECT);
  g_value_set_object(&value, task);
  return gdk_content_provider_new_for_value(&value);
}

static void on_drag_begin(GtkDragSource *source, GdkDrag *drag, ErrandsTask *row) {
  // char label[21];
  // if (strlen(row->data->text) > 20)
  //   snprintf(label, 17, "%s...", row->data->text);
  // else
  //   strcpy(label, row->data->text);
  // g_object_set(gtk_drag_icon_get_for_drag(drag), "child",
  //              g_object_new(GTK_TYPE_BUTTON, "label", label, NULL), NULL);
}

static void on_drag_end(GtkDragSource *self, GdkDrag *drag, gboolean delete_data, ErrandsTask *task) {
  g_object_set(task, "sensitive", true, NULL);
}

static gboolean on_drag_cancel(GtkDragSource *self, GdkDrag *drag, GdkDragCancelReason *reason, ErrandsTask *task) {
  g_object_set(task, "sensitive", true, NULL);
  return true;
}

static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTask *target_task) {
  // ErrandsTask *task = g_value_get_object(value);
  // if (!ERRANDS_IS_TASK(task))
  //   return false;
  // // Get old parent if task was sub-task
  // ErrandsTask *old_parent = ERRANDS_TASK(
  //     gtk_widget_get_ancestor(gtk_widget_get_parent(GTK_WIDGET(task)), ERRANDS_TYPE_TASK));
  // // Move widget
  // gtk_box_remove(GTK_BOX(gtk_widget_get_parent(GTK_WIDGET(task))), GTK_WIDGET(task));
  // gtk_box_prepend(GTK_BOX(target_task->sub_tasks), GTK_WIDGET(task));
  // // Set data
  // strcpy(task->data->parent, target_task->data->uid);
  // g_ptr_array_move_before(tdata, task->data, tdata->pdata[0]);
  // gtk_revealer_set_reveal_child(GTK_REVEALER(target_task->sub_tasks_revealer), true);
  // target_task->data->expanded = true;
  // errands_data_write();
  // // Toggle completion
  // if (target_task->data->completed && !task->data->completed)
  //   gtk_check_button_set_active(GTK_CHECK_BUTTON(target_task->complete_btn), false);
  // errands_task_list_sort_by_completion(target_task->sub_tasks);
  // // Update ui
  // errands_task_update_progress(target_task);
  // if (old_parent)
  //   errands_task_update_progress(old_parent);
  // // TODO: sync
  // return true;
}

static gboolean on_top_area_drop(GtkDropTarget *target, const GValue *value, double x, double y,
                                 ErrandsTask *target_task) {
  // ErrandsTask *task = g_value_get_object(value);
  // if (!ERRANDS_IS_TASK(task))
  //   return false;
  // ErrandsTask *target_parent = ERRANDS_TASK(
  //     gtk_widget_get_ancestor(gtk_widget_get_parent(GTK_WIDGET(target_task)),
  //     ERRANDS_TYPE_TASK));
  // // Get old parent if task was sub-task
  // ErrandsTask *old_parent = ERRANDS_TASK(
  //     gtk_widget_get_ancestor(gtk_widget_get_parent(GTK_WIDGET(task)), ERRANDS_TYPE_TASK));
  // // Move widget
  // gtk_box_remove(GTK_BOX(gtk_widget_get_parent(GTK_WIDGET(task))), GTK_WIDGET(task));
  // gtk_widget_insert_before(GTK_WIDGET(task), gtk_widget_get_parent(GTK_WIDGET(target_task)),
  //                          GTK_WIDGET(target_task));
  // // Set data
  // strcpy(task->data->parent, target_task->data->parent);
  // g_ptr_array_move_before(tdata, task->data, target_task->data);
  // errands_data_write();
  // if (target_parent) {
  //   if (!task->data->completed && target_parent->data->completed)
  //     gtk_check_button_set_active(GTK_CHECK_BUTTON(target_parent->complete_btn), false);
  //   errands_task_list_sort_by_completion(target_parent->sub_tasks);
  // } else
  //   errands_task_list_sort_by_completion(gtk_widget_get_parent(GTK_WIDGET(target_task)));
  // // Update ui
  // errands_task_update_progress(target_task);
  // if (old_parent)
  //   errands_task_update_progress(old_parent);
  // // TODO: sync
  // return true;
}
