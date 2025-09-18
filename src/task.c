#include "task.h"
#include "data/data.h"
#include "state.h"
#include "sync.h"
#include "utils.h"

#include "vendor/toolbox.h"
#include "window.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

// Callbacks
static void on_complete_btn_toggle_cb(GtkCheckButton *btn, ErrandsTask *task);
static void on_title_edit_cb(GtkEditableLabel *label, GParamSpec *pspec, gpointer user_data);
static void on_sub_task_entry_activated(GtkEntry *entry, ErrandsTask *task);
static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y, GtkPopover *popover);
static void on_toolbar_btn_toggle_cb(GtkToggleButton *btn, ErrandsTask *task);
static void on_errands_task_edited(GtkEditableLabel *entry, ErrandsTask *task);
static void on_errands_task_edit_cancelled(GtkButton *btn, ErrandsTask *task);

// Actions callbacks
static void on_action_edit(GSimpleAction *action, GVariant *param, ErrandsTask *task);
static void on_action_trash(GSimpleAction *action, GVariant *param, ErrandsTask *task);
static void on_action_clipboard(GSimpleAction *action, GVariant *param, ErrandsTask *task);
static void on_action_export(GSimpleAction *action, GVariant *param, ErrandsTask *task);

static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y, ErrandsTask *task);
static void on_drag_begin(GtkDragSource *source, GdkDrag *drag, ErrandsTask *task);
static void on_drag_end(GtkDragSource *self, GdkDrag *drag, gboolean delete_data, ErrandsTask *task);
static gboolean on_drag_cancel(GtkDragSource *self, GdkDrag *drag, GdkDragCancelReason *reason, ErrandsTask *task);
static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTask *task);

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
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, sub_entry);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_title_edit_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_right_click);
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

  self->complete_btn_signal_id =
      g_signal_connect(self->complete_btn, "toggled", G_CALLBACK(on_complete_btn_toggle_cb), self);
  self->toolbar_btn_signal_id =
      g_signal_connect(self->toolbar_btn, "toggled", G_CALLBACK(on_toolbar_btn_toggle_cb), self);

  // Actions
  errands_add_actions(GTK_WIDGET(self), "task", "edit", on_action_edit, self, "trash", on_action_trash, self,
                      "clipboard", on_action_clipboard, self, "export", on_action_export, self, NULL);

  // DND

  //
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

// ---------- PUBLIC FUNCTIONS ---------- //

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
  // Update UI
  // Show sub-tasks entry
  gtk_widget_set_visible(self->sub_entry, errands_data_get_bool(data, DATA_PROP_EXPANDED));
  errands_task_update_tags(self);
  errands_task_update_accent_color(self);
  errands_task_update_progress(self);
  errands_task_update_toolbar(self);
}

void errands_task_update_accent_color(ErrandsTask *task) {
  if (!task) return;
  const char *color = errands_data_get_str(task->data, DATA_PROP_COLOR);
  if (!g_str_equal(color, "none")) {
    const char *card_style = tb_tmp_str_printf("task-%s", color);
    const char *progress_style = tb_tmp_str_printf("progressbar-%s", color);
    const char *check_style = tb_tmp_str_printf("checkbtn-%s", color);
    gtk_widget_set_css_classes(GTK_WIDGET(task), (const char *[]){"card", card_style, NULL});
    gtk_widget_set_css_classes(GTK_WIDGET(task->complete_btn), (const char *[]){"selection-mode", check_style, NULL});
    gtk_widget_set_css_classes(GTK_WIDGET(task->progress_bar),
                               (const char *[]){"osd", "horizontal", progress_style, NULL});
  } else {
    gtk_widget_set_css_classes(GTK_WIDGET(task), (const char *[]){"card", NULL});
    gtk_widget_set_css_classes(GTK_WIDGET(task->complete_btn), (const char *[]){"selection-mode", NULL});
    gtk_widget_set_css_classes(GTK_WIDGET(task->progress_bar), (const char *[]){"osd", "horizontal", NULL});
  }
}

void errands_task_update_progress(ErrandsTask *task) {
  if (!task) return;
  GObject *model_item = g_object_get_data(G_OBJECT(task), "model-item");
  if (!model_item) return;
  GListModel *model = g_object_get_data(model_item, "children-model");
  if (!model) return;
  size_t total = 0;
  size_t completed = 0;
  for (size_t i = 0; i < g_list_model_get_n_items(model); ++i) {
    GObject *item = g_list_model_get_item(model, i);
    TaskData *td = g_object_get_data(item, "data");
    if (!errands_data_get_bool(td, DATA_PROP_DELETED) && !errands_data_get_bool(td, DATA_PROP_TRASH)) {
      total++;
      if (!icaltime_is_null_time(errands_data_get_time(td, DATA_PROP_COMPLETED_TIME))) completed++;
    }
  }
  gtk_revealer_set_reveal_child(GTK_REVEALER(task->progress_revealer), total > 0);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(task->progress_bar), total > 0 ? (float)completed / (float)total : 0);

  // TB_LOG_DEBUG("Task '%s' %d/%d", errands_data_get_str(task->data, DATA_PROP_TEXT), completed, total);
  // // Set sub-title
  // // if (total == 0) g_object_set(task->title_row, "subtitle", "", NULL);
  // // else {
  // //   g_autofree gchar *subtitle = g_strdup_printf(_("Completed: %zu / %zu"), completed, total);
  // //   g_object_set(task->title_row, "subtitle", subtitle, NULL);
  // // }
  // g_ptr_array_free(sub_tasks, false);
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
      const char *due_date_str = tb_tmp_str_printf("%s%s%s", due_ical_str, !strchr(due_ical_str, 'T') ? "T000000" : "",
                                                   !strchr(due_ical_str, 'Z') ? "Z" : "");
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

void errands_task_get_sub_tasks_tree(ErrandsTask *task, GPtrArray *array) {
  GtkTreeListRow *row = g_object_get_data(G_OBJECT(task), "row");
  if (!row) return;
  GListModel *children = gtk_tree_list_row_get_children(row);
  if (!children) return;
  for (size_t i = 0; i < g_list_model_get_n_items(children); ++i) {
    GObject *item = g_list_model_get_item(children, i);
    ErrandsTask *sub_task = g_object_get_data(item, "task");
    GtkTreeListRow *sub_row = g_object_get_data(G_OBJECT(sub_task), "row");
    if (sub_row) {
      g_ptr_array_add(array, sub_task);
      errands_task_get_sub_tasks_tree(sub_task, array);
    }
  }
}

// ---------- PRIVATE FUNCTIONS ---------- //

static ErrandsTask *get_parent_task(ErrandsTask *task) {
  GtkTreeListRow *row = g_object_get_data(G_OBJECT(task), "row");
  if (!row) return NULL;
  g_autoptr(GtkTreeListRow) parent_row = gtk_tree_list_row_get_parent(row);
  if (!parent_row) return NULL;
  g_autoptr(GObject) model_item = gtk_tree_list_row_get_item(parent_row);
  if (!model_item) return NULL;
  ErrandsTask *parent_task = g_object_get_data(model_item, "task");
  return parent_task;
}

static void errands_task_get_parents(ErrandsTask *task, GPtrArray *array) {
  ErrandsTask *parent = get_parent_task(task);
  if (parent) {
    g_ptr_array_add(array, parent);
    errands_task_get_parents(parent, array);
  }
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
  g_object_set(btn, "tooltip-text", _("Delete Tag"), NULL);
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
  static char str[256];
  if (task) {
    const char *uid = errands_data_get_str(task->data, DATA_PROP_UID);
    snprintf(str, sizeof(str), "ErrandsTask(%s, %p)", uid ? uid : "NULL", task);
  } else sprintf(str, "ErrandsTask(NULL)");
  return str;
}

// ---------- CALLBACKS ---------- //

// NOTE: maybe this function is not optimized, but it always been the case.
// Maybe need to look into it more.
static void on_complete_btn_toggle_cb(GtkCheckButton *btn, ErrandsTask *task) {
  tb_log("Toggle completion '%s'", errands_data_get_str(task->data, DATA_PROP_UID));
  bool active = gtk_check_button_get_active(btn);
  errands_data_set_time(task->data, DATA_PROP_COMPLETED_TIME,
                        active ? icaltime_get_date_time_now() : icaltime_null_time());
  // Complete all sub-tasks if checked
  if (active) {
    GPtrArray *sub_tasks_tree = g_ptr_array_new();
    task_data_get_sub_tasks_tree(task->data, sub_tasks_tree);
    for (size_t i = 0; i < sub_tasks_tree->len; ++i) {
      TaskData *sub_task_data = sub_tasks_tree->pdata[i];
      errands_data_set_time(sub_task_data, DATA_PROP_COMPLETED_TIME, icaltime_get_date_time_now());
    }
    g_ptr_array_free(sub_tasks_tree, false);
    sub_tasks_tree = g_ptr_array_new();
    errands_task_get_sub_tasks_tree(task, sub_tasks_tree);
    for (size_t i = 0; i < sub_tasks_tree->len; ++i) {
      ErrandsTask *sub_task = sub_tasks_tree->pdata[i];
      errands_task_set_data(sub_task, sub_task->data);
    }
    g_ptr_array_free(sub_tasks_tree, false);
  }
  // Uncomplete parent tasks if unchecked
  else {
    GPtrArray *parents = g_ptr_array_new();
    errands_task_get_parents(task, parents);
    for (size_t i = 0; i < parents->len; ++i) {
      ErrandsTask *parent = parents->pdata[i];
      bool completed = gtk_check_button_get_active(GTK_CHECK_BUTTON(parent->complete_btn));
      if (completed) {
        errands_data_set_time(parent->data, DATA_PROP_COMPLETED_TIME, icaltime_null_time());
        errands_task_set_data(parent, parent->data);
      }
      errands_task_update_progress(parent);
    }
    g_ptr_array_free(parents, false);
  }
  errands_data_write_list(task_data_get_list(task->data));
  errands_task_update_progress(task);
  errands_task_update_progress(get_parent_task(task));

  // Sort task list by completion
  gtk_sorter_changed(GTK_SORTER(state.main_window->task_list->sorter), GTK_SORTER_CHANGE_MORE_STRICT);
  // TODO: remember next row and scroll to it

  // Update task list
  errands_task_list_update_title();
  errands_sidebar_all_row_update_counter(state.main_window->sidebar->all_row);
  errands_sidebar_today_row_update_counter(state.main_window->sidebar->today_row);
  errands_sidebar_task_list_row_update_counter(
      errands_sidebar_task_list_row_get(errands_data_get_str(task->data, DATA_PROP_LIST_UID)));

  needs_sync = true;
}

static void on_title_edit_cb(GtkEditableLabel *label, GParamSpec *pspec, gpointer user_data) {
  bool editing = gtk_editable_label_get_editing(label);
  tb_log("Task: Edit '%s'", editing ? "on" : "off");
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
    needs_sync = true;
  }
}

static void on_toolbar_btn_toggle_cb(GtkToggleButton *btn, ErrandsTask *task) {
  tb_log("Task '%s': Toggle toolbar", errands_data_get_str(task->data, DATA_PROP_UID));
  errands_data_set_bool(task->data, DATA_PROP_TOOLBAR_SHOWN, gtk_toggle_button_get_active(btn));
  errands_data_write_list(task_data_get_list(task->data));
}

static void on_sub_task_entry_activated(GtkEntry *entry, ErrandsTask *task) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (g_str_equal(text, "")) return;
  // Add new task data
  TaskData *new_td = list_data_create_task(state.main_window->task_list->data, (char *)text,
                                           errands_data_get_str(task->data, DATA_PROP_LIST_UID),
                                           errands_data_get_str(task->data, DATA_PROP_UID));
  g_hash_table_insert(tdata, g_strdup(errands_data_get_str(new_td, DATA_PROP_UID)), new_td);
  // Add new model item
  GObject *model_item = g_object_get_data(G_OBJECT(task), "model-item");
  GObject *children_model = g_object_get_data(G_OBJECT(model_item), "children-model");
  GObject *data_object = task_data_as_gobject(new_td);
  g_list_store_append(G_LIST_STORE(children_model), data_object);
  // Expand task row
  errands_data_set_bool(task->data, DATA_PROP_EXPANDED, true);
  errands_data_write_list(state.main_window->task_list->data);
  // Reset text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  tb_log("Task '%s': Add sub-task '%s'", errands_data_get_str(task->data, DATA_PROP_UID),
         errands_data_get_str(new_td, DATA_PROP_UID));
  errands_task_update_progress(task);
  needs_sync = true;
}

static void on_right_click(GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y, GtkPopover *popover) {
  gtk_popover_set_pointing_to(popover, &(GdkRectangle){x, y});
  gtk_popover_popup(popover);
}

// --- ACTIONS CALLBACKS --- //

static void on_action_edit(GSimpleAction *action, GVariant *param, ErrandsTask *task) {
  gtk_editable_label_start_editing(GTK_EDITABLE_LABEL(task->edit_title));
}

static void on_action_trash(GSimpleAction *action, GVariant *param, ErrandsTask *task) {
  errands_data_set_bool(task->data, DATA_PROP_TRASH, true);
  errands_data_write_list(task_data_get_list(task->data));
  errands_task_list_update_title();
  errands_sidebar_all_row_update_counter(state.main_window->sidebar->all_row);
  errands_sidebar_today_row_update_counter(state.main_window->sidebar->today_row);
  errands_sidebar_task_list_row_update_counter(
      errands_sidebar_task_list_row_get(errands_data_get_str(task->data, DATA_PROP_LIST_UID)));
}

static void on_action_clipboard(GSimpleAction *action, GVariant *param, ErrandsTask *task) {
  const char *text = errands_data_get_str(task->data, DATA_PROP_TEXT);
  g_autoptr(GdkClipboard) clipboard = gdk_display_get_clipboard(gtk_widget_get_display(GTK_WIDGET(task)));
  gdk_clipboard_set(clipboard, G_TYPE_STRING, text);
  errands_window_add_toast(state.main_window, _("Copied to Clipboard"));
}

static void on_action_export(GSimpleAction *action, GVariant *param, ErrandsTask *task) {
  icalcomponent *cal = icalcomponent_new_vcalendar();
  icalcomponent *dup = icalcomponent_new_clone(task->data);
  icalcomponent_add_component(cal, task->data);
  TB_TODO("Show export dialog here");
  icalcomponent_free(dup);
  icalcomponent_free(cal);
}

static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y, ErrandsTask *task) {
  g_object_set(task, "sensitive", false, NULL);
  GValue value = G_VALUE_INIT;
  g_value_init(&value, G_TYPE_OBJECT);
  g_value_set_object(&value, task);
  return gdk_content_provider_new_for_value(&value);
}

// --- DND CALLBACKS --- //

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
