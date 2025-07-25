#include "task.h"
#include "data/data.h"
#include "state.h"
#include "task-list.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

// ---------- SIGNALS ---------- //

static void on_complete_btn_toggle_cb(GtkCheckButton *btn, ErrandsTask *task);
static void on_title_edit_cb(GtkEditableLabel *label, GParamSpec *pspec, gpointer user_data);
static void on_expand_toggle_cb(GtkGestureClick *self, gint n_press, gdouble x, gdouble y, ErrandsTask *task);
static void on_sub_task_entry_activated(GtkEntry *entry, ErrandsTask *task);
static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y, GtkPopover *popover);
static void on_action_edit(GSimpleAction *action, GVariant *param, ErrandsTask *task);
static void on_action_trash(GSimpleAction *action, GVariant *param, ErrandsTask *task);
static void on_toolbar_btn_toggle_cb(GtkToggleButton *btn, ErrandsTask *task);
static void on_errands_task_edited(GtkEditableLabel *entry, ErrandsTask *task);
static void on_errands_task_edit_cancelled(GtkButton *btn, ErrandsTask *task);

static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y, ErrandsTask *task);
static void on_drag_begin(GtkDragSource *source, GdkDrag *drag, ErrandsTask *task);
static void on_drag_end(GtkDragSource *self, GdkDrag *drag, gboolean delete_data, ErrandsTask *task);
static gboolean on_drag_cancel(GtkDragSource *self, GdkDrag *drag, GdkDragCancelReason *reason, ErrandsTask *task);
static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTask *task);
static gboolean on_top_area_drop(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTask *task);

// static bool sub_tasks_filter_func(GObject *obj, TaskData *parent_data) {
//   if (!parent_data) return false;
//   TaskData *child_data = g_object_get_data(G_OBJECT(obj), "data");
//   const char *child_parent = errands_data_get_str(child_data, DATA_PROP_PARENT);
//   const char *parent_uid = errands_data_get_str(parent_data, DATA_PROP_UID);
//   bool deleted = errands_data_get_bool(child_data, DATA_PROP_DELETED);
//   bool trash = errands_data_get_bool(child_data, DATA_PROP_TRASH);
//   return child_parent && g_str_equal(child_parent, parent_uid) && !deleted && !trash;
// }

// static GtkWidget *create_widget_func(GObject *item, gpointer user_data) {
//   ErrandsTask *task = errands_task_new();
//   errands_task_set_data(task, g_object_get_data(item, "data"));
//   return GTK_WIDGET(task);
// }

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTask, errands_task, GTK_TYPE_BOX)

static void errands_task_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK);
  G_OBJECT_CLASS(errands_task_parent_class)->dispose(gobject);
}

static void errands_task_class_init(ErrandsTaskClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_task_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), "/io/github/mrvladus/Errands/ui/task.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, complete_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, title);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, edit_title);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, toolbar_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, tags_revealer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, tags_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, progress_revealer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, progress_bar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, toolbar_revealer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, date_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, date_btn_content);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, notes_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, priority_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, tags_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, attachments_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, color_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, sub_tasks_revealer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, sub_entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, task_list);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_title_edit_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_expand_toggle_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_date_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_notes_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_priority_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_tags_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_attachments_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_color_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_sub_task_entry_activated);
}

static void errands_task_init(ErrandsTask *self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  // Top drop area
  // GtkWidget *top_drop_area = gtk_image_new_from_icon_name("errands-add-symbolic");
  // gtk_widget_add_css_class(top_drop_area, "task-top-drop-area");
  // GtkDropTarget *top_drop_area_target = gtk_drop_target_new(G_TYPE_OBJECT, GDK_ACTION_MOVE);
  // g_signal_connect(top_drop_area_target, "drop", G_CALLBACK(on_top_area_drop), self);
  // gtk_widget_add_controller(GTK_WIDGET(top_drop_area), GTK_EVENT_CONTROLLER(top_drop_area_target));
  // g_object_set(top_drop_area, "margin-start", 12, "margin-end", 12, NULL);
  // gtk_box_append(GTK_BOX(main_box), top_drop_area);

  self->complete_btn_signal_id =
      g_signal_connect(self->complete_btn, "toggled", G_CALLBACK(on_complete_btn_toggle_cb), self);
  self->toolbar_btn_signal_id =
      g_signal_connect(self->toolbar_btn, "toggled", G_CALLBACK(on_toolbar_btn_toggle_cb), self);

  // Right-click menu
  // g_autoptr(GMenu) menu = errands_menu_new(2, _("Edit"), "task.edit", _("Move to Trash"), "task.trash");

  // Menu popover
  // GtkWidget *menu_popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
  // g_object_set(menu_popover, "has-arrow", false, "halign", GTK_ALIGN_START, NULL);
  // gtk_box_append(GTK_BOX(vbox), menu_popover);

  // Right-click controllers
  // GtkGesture *ctrl = gtk_gesture_click_new();
  // gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(ctrl), 3);
  // g_signal_connect(ctrl, "released", G_CALLBACK(on_right_click), menu_popover);
  // gtk_widget_add_controller(title_box, GTK_EVENT_CONTROLLER(ctrl));
  // GtkGesture *touch_ctrl = gtk_gesture_long_press_new();
  // g_signal_connect(touch_ctrl, "pressed", G_CALLBACK(on_right_click), menu_popover);
  // gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(touch_ctrl), true);
  // gtk_widget_add_controller(title_box, GTK_EVENT_CONTROLLER(touch_ctrl));

  // Actions
  errands_add_actions(GTK_WIDGET(self), "task", "edit", on_action_edit, self, "trash", on_action_trash, self, NULL);

  // DND
  // GtkDragSource *drag_source = gtk_drag_source_new();
  // gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
  // g_signal_connect(drag_source, "prepare", G_CALLBACK(on_drag_prepare), self);
  // g_signal_connect(drag_source, "drag-begin", G_CALLBACK(on_drag_begin), self);
  // g_signal_connect(drag_source, "drag-end", G_CALLBACK(on_drag_end), self);
  // g_signal_connect(drag_source, "drag-cancel", G_CALLBACK(on_drag_cancel), self);
  // gtk_widget_add_controller(GTK_WIDGET(self->title_row), GTK_EVENT_CONTROLLER(drag_source));

  // GtkDropTarget *drop_target = gtk_drop_target_new(G_TYPE_OBJECT, GDK_ACTION_MOVE);
  // g_signal_connect(drop_target, "drop", G_CALLBACK(on_drop), self);
  // gtk_widget_add_controller(GTK_WIDGET(self->title_row), GTK_EVENT_CONTROLLER(drop_target));

  // GtkEventController *drop_ctrl = gtk_drop_controller_motion_new();
  // g_object_bind_property(drop_ctrl, "contains-pointer", top_drop_area, "visible", G_BINDING_SYNC_CREATE);
  // gtk_widget_add_controller(GTK_WIDGET(self), drop_ctrl);
}

ErrandsTask *errands_task_new() { return g_object_new(ERRANDS_TYPE_TASK, NULL); }

void errands_task_set_data(ErrandsTask *self, TaskData *data) {
  self->data = data;

  // Set text
  gtk_label_set_label(GTK_LABEL(self->title), errands_data_get_str(data, DATA_PROP_TEXT));
  // Set completion
  g_signal_handler_block(self->complete_btn, self->complete_btn_signal_id);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(self->complete_btn),
                              !icaltime_is_null_time(errands_data_get_time(data, DATA_PROP_COMPLETED_TIME)));
  g_signal_handler_unblock(self->complete_btn, self->complete_btn_signal_id);
  // Set toolbar
  g_signal_handler_block(self->toolbar_btn, self->toolbar_btn_signal_id);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->toolbar_btn),
                               errands_data_get_bool(data, DATA_PROP_TOOLBAR_SHOWN));
  g_signal_handler_unblock(self->toolbar_btn, self->toolbar_btn_signal_id);
  // Set sub-tasks
  gtk_revealer_set_reveal_child(GTK_REVEALER(self->sub_tasks_revealer),
                                errands_data_get_bool(data, DATA_PROP_EXPANDED));
  // Load sub-tasks
  // LOG("Loading sub-tasks for %s", errands_task_as_str(task));
  // GPtrArray *tasks = g_hash_table_get_values_as_ptr_array(tdata);
  // const char *uid = errands_data_get_str(task->data, DATA_PROP_UID);
  // for (size_t i = 0; i < tasks->len; i++) {
  //   TaskData *data = tasks->pdata[i];
  //   const char *parent = errands_data_get_str(data, DATA_PROP_PARENT);
  //   bool trash = errands_data_get_bool(data, DATA_PROP_TRASH);
  //   bool deleted = errands_data_get_bool(data, DATA_PROP_DELETED);
  //   if (!deleted && !trash && (parent && g_str_equal(parent, uid)))
  //     gtk_list_box_append(GTK_LIST_BOX(task->task_list), GTK_WIDGET(errands_task_new(tasks->pdata[i])));
  // }
  // g_ptr_array_free(tasks, false);
  // task->sub_tasks_filter = gtk_custom_filter_new((GtkCustomFilterFunc)sub_tasks_filter_func, data, NULL);
  // task->sub_tasks_filter_model =
  // gtk_filter_list_model_new(G_LIST_MODEL(state.main_window->task_list->completion_sort_model),
  //                                                          GTK_FILTER(task->sub_tasks_filter));
  // gtk_list_box_bind_model(GTK_LIST_BOX(task->task_list), G_LIST_MODEL(task->sub_tasks_filter_model),
  //                         (GtkListBoxCreateWidgetFunc)create_widget_func, NULL, NULL);

  // gtk_filter_changed(GTK_FILTER(task->sub_tasks_filter), GTK_FILTER_CHANGE_DIFFERENT);

  errands_task_update_tags(self);
  errands_task_update_accent_color(self);
  errands_task_update_progress(self);
  errands_task_update_toolbar(self);
}

void errands_task_update_accent_color(ErrandsTask *task) {
  const char *color = errands_data_get_str(task->data, DATA_PROP_COLOR);
  if (!g_str_equal(color, "none")) {
    g_autofree gchar *accent_style = g_strdup_printf("task-%s", color);
    gtk_widget_set_css_classes(GTK_WIDGET(task), (const char *[]){"card", accent_style, NULL});
  } else {
    gtk_widget_set_css_classes(GTK_WIDGET(task), (const char *[]){"card", NULL});
  }
}

void errands_task_update_progress(ErrandsTask *task) {
  GPtrArray *sub_tasks = get_children(task->task_list);
  size_t total = 0;
  size_t completed = 0;
  for (size_t i = 0; i < sub_tasks->len; i++) {
    ErrandsTask *sub_task = sub_tasks->pdata[i];
    TaskData *td = sub_task->data;
    if (!errands_data_get_bool(td, DATA_PROP_DELETED) && !errands_data_get_bool(td, DATA_PROP_TRASH)) {
      total++;
      if (!icaltime_is_null_time(errands_data_get_time(td, DATA_PROP_COMPLETED_TIME))) completed++;
    }
  }
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->progress_revealer), total > 0);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(task->progress_bar), total > 0 ? (float)completed / (float)total : 0);

  // Set sub-title
  // if (total == 0) g_object_set(task->title_row, "subtitle", "", NULL);
  // else {
  //   g_autofree gchar *subtitle = g_strdup_printf(_("Completed: %zu / %zu"), completed, total);
  //   g_object_set(task->title_row, "subtitle", subtitle, NULL);
  // }
  g_ptr_array_free(sub_tasks, false);
}

void errands_task_update_toolbar(ErrandsTask *task) {
  TaskData *data = task->data;
  // Update css for buttons
  // Notes button
  if (errands_data_get_str(data, DATA_PROP_NOTES)) gtk_widget_add_css_class(task->notes_btn, "accent");
  // Attachments button
  g_auto(GStrv) attachments = errands_data_get_strv(data, DATA_PROP_ATTACHMENTS);
  if (attachments && g_strv_length(attachments) > 0)
    gtk_widget_add_css_class(GTK_WIDGET(task->attachments_btn), "accent");
  // Priority button
  uint8_t priority = errands_data_get_int(data, DATA_PROP_PRIORITY);
  gtk_button_set_icon_name(GTK_BUTTON(task->priority_btn),
                           priority > 0 ? "errands-priority-set-symbolic" : "errands-priority-symbolic");
  if (priority > 0 && priority < 3) gtk_widget_add_css_class(task->priority_btn, "priority-low");
  else if (priority >= 3 && priority < 7) gtk_widget_add_css_class(task->priority_btn, "priority-medium");
  else if (priority >= 7 && priority < 10) gtk_widget_add_css_class(task->priority_btn, "priority-high");
  // Update date button
  icaltimetype due_dt = errands_data_get_time(data, DATA_PROP_DUE_TIME);
  icalproperty *rrule_prop = icalcomponent_get_first_property(data, ICAL_RRULE_PROPERTY);
  if (rrule_prop) {
    struct icalrecurrencetype rrule = icalproperty_get_rrule(rrule_prop);
    g_autoptr(GString) label = g_string_new(NULL);
    switch (rrule.freq) {
    case ICAL_SECONDLY_RECURRENCE: g_string_append_printf(label, _("Every %d seconds"), rrule.interval); break;
    case ICAL_MINUTELY_RECURRENCE: g_string_append_printf(label, _("Every %d minutes"), rrule.interval); break;
    case ICAL_HOURLY_RECURRENCE: g_string_append_printf(label, _("Every %d hours"), rrule.interval); break;
    case ICAL_DAILY_RECURRENCE: g_string_append_printf(label, _("Every %d days"), rrule.interval); break;
    case ICAL_WEEKLY_RECURRENCE: g_string_append_printf(label, _("Every %d weeks"), rrule.interval); break;
    case ICAL_MONTHLY_RECURRENCE: g_string_append_printf(label, _("Every %d months"), rrule.interval); break;
    case ICAL_YEARLY_RECURRENCE: g_string_append_printf(label, _("Every %d years"), rrule.interval); break;
    case ICAL_NO_RECURRENCE: break;
    }
    if (!icaltime_is_null_time(rrule.until)) {
      g_autoptr(GDateTime) dt = g_date_time_new_from_unix_local(icaltime_as_timet(rrule.until));
      g_autofree gchar *dt_str = g_date_time_format(dt, "%x");
      g_string_append_printf(label, _(" until %s"), dt_str);
    } else {
      if (rrule.count > 0) g_string_append_printf(label, _(" %d times"), rrule.count);
    }
    g_object_set(task->date_btn_content, "label", label->str ? label->str : _("Date"), NULL);
  } else {
    if (icaltime_is_null_date(due_dt)) g_object_set(task->date_btn_content, "label", _("Date"), NULL);
    else {
      const char *due_ical_str = icaltime_as_ical_string(due_dt);
      g_autofree gchar *due_date_str = g_strdup_printf(
          "%s%s%s", due_ical_str, !strchr(due_ical_str, 'T') ? "T000000" : "", !strchr(due_ical_str, 'Z') ? "Z" : "");
      g_autoptr(GTimeZone) tz = g_time_zone_new_local();
      g_autoptr(GDateTime) dt = g_date_time_new_from_iso8601(due_date_str, tz);
      g_autofree gchar *date_str = NULL;
      if (strchr(due_ical_str, 'T')) date_str = g_date_time_format(dt, "%d %b %H:%M");
      else date_str = g_date_time_format(dt, "%d %b");
      g_object_set(task->date_btn_content, "label", date_str, NULL);
    }
  }
  // Set style for date button
  gtk_widget_remove_css_class(task->date_btn, "error");
  if (!icaltime_is_null_date(due_dt) && icaltime_compare_date_only(due_dt, icaltime_today()) <= 0)
    gtk_widget_add_css_class(task->date_btn, "error");
}

static void __append_sub_tasks(GPtrArray *arr, ErrandsTask *task) {
  GPtrArray *sub_tasks = get_children(task->task_list);
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

// --- TAGS --- //

static void errands_task_tag_delete(GtkWidget *label) {
  ErrandsTask *task = (ErrandsTask *)gtk_widget_get_ancestor(label, ERRANDS_TYPE_TASK);
  errands_data_remove_tag(task->data, DATA_PROP_TAGS, gtk_label_get_label(GTK_LABEL(label)));
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
  // Remove all tags
  for (GtkWidget *child = gtk_widget_get_first_child(task->tags_box); child;
       child = gtk_widget_get_first_child(task->tags_box))
    adw_wrap_box_remove(ADW_WRAP_BOX(task->tags_box), child);
  // Add tags
  g_auto(GStrv) tags = errands_data_get_strv(task->data, DATA_PROP_TAGS);
  const size_t len = tags ? g_strv_length(tags) : 0;
  for_range(i, 0, len) adw_wrap_box_append(ADW_WRAP_BOX(task->tags_box), errands_task_tag_new(tags[i]));
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->tags_revealer), len > 0);
}

const char *errands_task_as_str(ErrandsTask *task) {
  static char str[512];
  if (task) {
    const char *uid = errands_data_get_str(task->data, DATA_PROP_UID);
    snprintf(str, sizeof(str), "ErrandsTask(%s, %p)", uid ? uid : "NULL", task);
  } else sprintf(str, "ErrandsTask(NULL)");
  return str;
}

// ---------- CALLBACKS ---------- //

static void on_complete_btn_toggle_cb(GtkCheckButton *btn, ErrandsTask *task) {
  LOG("Toggle completion '%s'", errands_data_get_str(task->data, DATA_PROP_UID));
  // errands_data_set_str(task->data, DATA_PROP_COMPLETED, gtk_check_button_get_active(btn) ? get_date_time() : NULL);
  errands_data_write_list(state.main_window->task_list->data);

  // GtkWidget *task_list;
  // if (!strcmp(errands_data_get_str(task->data, DATA_PROP_PARENT), "")) task_list =
  // state.main_window->task_list->task_list; else task_list = gtk_widget_get_parent(GTK_WIDGET(task));

  // Complete all sub-tasks if task is completed
  // if (errands_data_get_str(task->data, DATA_PROP_COMPLETED)) {
  //   GPtrArray *sub_tasks = get_children(task->sub_tasks);
  //   for (size_t i = 0; i < sub_tasks->len; i++) {
  //     ErrandsTask *sub_task = sub_tasks->pdata[i];
  //     gtk_check_button_set_active(GTK_CHECK_BUTTON(sub_task->complete_btn), true);
  //   }
  // }

  // Update parents
  // GPtrArray *parents = errands_task_get_parents(task);
  // for (size_t i = 0; i < parents->len; i++) {
  //   ErrandsTask *parent = parents->pdata[i];
  //   errands_task_update_progress(parent);
  //   // Uncomplete parent task if task is un-completed
  //   if (!gtk_check_button_get_active(btn)) gtk_check_button_set_active(GTK_CHECK_BUTTON(parent->complete_btn),
  //   false);
  // }

  // gtk_revealer_set_reveal_child(GTK_REVEALER(task->revealer),
  //                               !errands_data_get_bool(task->data, DATA_PROP_DELETED) &&
  //                                   !errands_data_get_bool(task->data, DATA_PROP_TRASH) &&
  //                                   (!errands_data_get_str(task->data, DATA_PROP_COMPLETED) ||
  //                                    errands_settings_get("show_completed", SETTING_TYPE_BOOL).b));

  // Sort parent task list by completion
  if (!errands_data_get_str(task->data, DATA_PROP_PARENT)) {
    gtk_sorter_changed(GTK_SORTER(state.main_window->task_list->completion_sorter), GTK_SORTER_CHANGE_MORE_STRICT);
  }

  // Update task list
  errands_task_list_update_title();
  errands_sidebar_all_row_update_counter(state.main_window->sidebar->all_row);
  errands_sidebar_task_list_row_update_counter(
      errands_sidebar_task_list_row_get(errands_data_get_str(task->data, DATA_PROP_LIST_UID)));
}

static void on_title_edit_cb(GtkEditableLabel *label, GParamSpec *pspec, gpointer user_data) {
  bool editing = gtk_editable_label_get_editing(label);
  LOG("Task: Edit '%s'", editing ? "on" : "off");
  ErrandsTask *task = user_data;
  const char *curr_text = errands_data_get_str(task->data, DATA_PROP_TEXT);
  if (editing) {
    gtk_widget_set_visible(GTK_WIDGET(label), true);
    gtk_editable_set_text(GTK_EDITABLE(task->edit_title), curr_text);
    gtk_widget_grab_focus(task->edit_title);
  } else {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(task->edit_title));
    if (!text || g_str_equal("", text)) {
      gtk_widget_set_visible(task->title, true);
      return;
    }
    if (g_str_equal(text, curr_text)) {
      gtk_widget_set_visible(task->title, true);
      return;
    }
    gtk_widget_set_visible(task->title, true);
    errands_data_set_str(task->data, DATA_PROP_TEXT, text);
    errands_data_write_list(task_data_get_list(task->data));
    gtk_label_set_label(GTK_LABEL(task->title), text);
    // TODO: sync
  }
}

static void on_expand_toggle_cb(GtkGestureClick *self, gint n_press, gdouble x, gdouble y, ErrandsTask *task) {
  const bool expanded = errands_data_get_bool(task->data, DATA_PROP_EXPANDED);
  errands_data_set_bool(task->data, DATA_PROP_EXPANDED, !expanded);
  errands_data_write_list(task_data_get_list(task->data));
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->sub_tasks_revealer), !expanded);
}

static void on_toolbar_btn_toggle_cb(GtkToggleButton *btn, ErrandsTask *task) {
  LOG("Task %s: Toggle toolbar", errands_data_get_str(task->data, DATA_PROP_UID));
  errands_data_set_bool(task->data, DATA_PROP_TOOLBAR_SHOWN, gtk_toggle_button_get_active(btn));
  errands_data_write_list(task_data_get_list(task->data));
}

static void on_sub_task_entry_activated(GtkEntry *entry, ErrandsTask *task) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (g_str_equal(text, "")) return;
  TaskData *new_td = list_data_create_task(state.main_window->task_list->data, (char *)text,
                                           errands_data_get_str(task->data, DATA_PROP_LIST_UID),
                                           errands_data_get_str(task->data, DATA_PROP_UID));
  errands_data_write_list(state.main_window->task_list->data);
  GObject *data_object = g_object_new(G_TYPE_OBJECT, NULL);
  g_object_set_data(data_object, "data", new_td);
  g_list_store_append(state.main_window->task_list->tasks_model, data_object);
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
}

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y, GtkPopover *popover) {
  gtk_popover_set_pointing_to(popover, &(GdkRectangle){.x = x, .y = y});
  gtk_popover_popup(popover);
}

static void on_action_edit(GSimpleAction *action, GVariant *param, ErrandsTask *task) {
  // gtk_editable_label_start_editing(GTK_EDITABLE_LABEL(task->edit_title));
}

static void on_action_trash(GSimpleAction *action, GVariant *param, ErrandsTask *task) {
  errands_data_set_bool(task->data, DATA_PROP_TRASH, true);
  errands_data_write_list(task_data_get_list(task->data));
  gtk_widget_set_visible(GTK_WIDGET(task), false);
  errands_task_list_update_title();
  errands_sidebar_all_row_update_counter(state.main_window->sidebar->all_row);
  errands_sidebar_task_list_row_update_counter(
      errands_sidebar_task_list_row_get(errands_data_get_str(task->data, DATA_PROP_LIST_UID)));
}

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
  return true;
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
  return true;
}
