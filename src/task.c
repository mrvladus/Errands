#include "task.h"
#include "data.h"
#include "gtk/gtk.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"
#include "task-list.h"
#include "window.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static GtkWidget *errands_task_tag_new(ErrandsTask *self, const char *tag);

// Callbacks
static void on_complete_btn_toggle_cb(ErrandsTask *self, GtkCheckButton *btn);
static void on_unpin_btn_clicked_cb(ErrandsTask *self, GtkToggleButton *btn);
static void on_title_edit_cb(GtkEditableLabel *label, GParamSpec *pspec, gpointer user_data);
static void on_sub_task_entry_activated(GtkEntry *entry, ErrandsTask *self);
static void on_expand_toggle_cb(ErrandsTask *self, GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y);

static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y, ErrandsTask *self);
static void on_drag_begin(GtkDragSource *source, GdkDrag *drag, ErrandsTask *self);
static void on_drag_end(GtkDragSource *source, GdkDrag *drag, gboolean delete_data, ErrandsTask *self);
static gboolean on_drag_cancel(GtkDragSource *source, GdkDrag *drag, GdkDragCancelReason *reason, ErrandsTask *self);
static gboolean on_drop(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTask *self);

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
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, toolbar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, props_bar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, tags_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, progress_bar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, date_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, unpin_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, date_btn_content);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, notes_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, priority_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, attachments_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, attachments_count);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTask, sub_entry);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_title_edit_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_unpin_btn_clicked_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_complete_btn_toggle_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_date_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_notes_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_priority_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_tags_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_attachments_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_list_color_dialog_show);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_sub_task_entry_activated);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_expand_toggle_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), errands_task_menu_show);
}

static void errands_task_init(ErrandsTask *self) {
  gtk_widget_init_template(GTK_WIDGET(self));

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
  gtk_widget_set_visible(GTK_WIDGET(self), data ? true : false);
  if (!data) return;
  self->data = data;
  gtk_widget_set_sensitive(GTK_WIDGET(self), !errands_data_get_bool(data->data, DATA_PROP_CANCELLED));
  // Set text
  gtk_label_set_label(GTK_LABEL(self->title), errands_data_get_str(data->data, DATA_PROP_TEXT));
  // Set completion
  g_signal_handlers_block_by_func(self->complete_btn, on_complete_btn_toggle_cb, self);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(self->complete_btn),
                              !icaltime_is_null_time(errands_data_get_time(data->data, DATA_PROP_COMPLETED_TIME)));
  g_signal_handlers_unblock_by_func(self->complete_btn, on_complete_btn_toggle_cb, self);
  // Update UI
  // Show sub-tasks entry
  gtk_widget_set_visible(self->sub_entry, errands_data_get_bool(data->data, DATA_PROP_EXPANDED));
  errands_task_update_accent_color(self);
  errands_task_update_progress(self);
  errands_task_update_toolbar(self);
}

void errands_task_update_accent_color(ErrandsTask *task) {
  if (!task) return;
  const char *color = errands_data_get_str(task->data->data, DATA_PROP_COLOR);
  if (!STR_EQUAL(color, "none")) {
    const char *card_style = tmp_str_printf("task-%s", color);
    const char *progress_style = tmp_str_printf("progressbar-%s", color);
    const char *check_style = tmp_str_printf("checkbtn-%s", color);
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

void errands_task_update_progress(ErrandsTask *self) {
  if (!self) return;
  size_t total = 0, completed = 0;
  for_range(i, 0, self->data->children->len) {
    TaskData *data = g_ptr_array_index(self->data->children, i);
    CONTINUE_IF(errands_data_get_bool(data->data, DATA_PROP_DELETED));
    if (!icaltime_is_null_time(errands_data_get_time(data->data, DATA_PROP_COMPLETED_TIME))) completed++;
    total++;
  }
  gtk_widget_set_visible(self->progress_bar, total > 0);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(self->progress_bar), total > 0 ? (float)completed / (float)total : 0);
  gtk_widget_set_tooltip_text(self->progress_bar,
                              total == 0 ? "" : tmp_str_printf(_("Completed: %zu / %zu"), completed, total));
}

void errands_task_update_toolbar(ErrandsTask *task) {
  TaskData *data = task->data;
  gtk_widget_set_visible(task->unpin_btn, errands_data_get_bool(data->data, DATA_PROP_PINNED));
  // Notes button
  bool has_notes = errands_data_get_str(data->data, DATA_PROP_NOTES) != NULL;
  gtk_widget_set_visible(task->notes_btn, has_notes);
  // Attachments button
  g_auto(GStrv) attachments = errands_data_get_strv(data->data, DATA_PROP_ATTACHMENTS);
  bool has_attachments = attachments && g_strv_length(attachments) > 0;
  gtk_widget_set_visible(task->attachments_btn, has_attachments);
  gtk_label_set_label(task->attachments_count,
                      has_attachments ? tmp_str_printf("%zu", g_strv_length(attachments)) : "");
  // Priority button
  uint8_t priority = errands_data_get_int(data->data, DATA_PROP_PRIORITY);
  const char *priority_class = NULL;
  if (priority < 3) priority_class = "accent";
  else if (priority >= 3 && priority < 7) priority_class = "warning";
  else if (priority >= 7 && priority < 10) priority_class = "error";
  gtk_widget_set_css_classes(task->priority_btn, (const char *[]){"image-button", priority_class, NULL});
  gtk_widget_set_visible(task->priority_btn, priority > 0);
  // Update date button text
  icaltimetype due_dt = errands_data_get_time(data->data, DATA_PROP_DUE_TIME);
  bool has_due_date = !icaltime_is_null_date(due_dt);
  gtk_widget_set_visible(task->date_btn, has_due_date);
  icalproperty *rrule_prop = icalcomponent_get_first_property(data->data, ICAL_RRULE_PROPERTY);
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
      const char *due_date_str = tmp_str_printf("%s%s%s", due_ical_str, !strchr(due_ical_str, 'T') ? "T000000" : "",
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
  gtk_widget_set_css_classes(
      task->date_btn, (const char *[]){"image-button", "caption", errands_task_data_is_due(data) ? "error" : "", NULL});

  bool props_bar_visible = has_notes || has_attachments || has_due_date;
  gtk_widget_set_visible(task->props_bar, props_bar_visible);

  // Update tags
  for (GtkWidget *child = gtk_widget_get_first_child(task->tags_box); child;
       child = gtk_widget_get_first_child(task->tags_box))
    adw_wrap_box_remove(ADW_WRAP_BOX(task->tags_box), child);
  g_auto(GStrv) tags = errands_data_get_strv(task->data->data, DATA_PROP_TAGS);
  const size_t tags_n = tags ? g_strv_length(tags) : 0;
  for_range(i, 0, tags_n) adw_wrap_box_append(ADW_WRAP_BOX(task->tags_box), errands_task_tag_new(task, tags[i]));

  gtk_widget_set_visible(task->toolbar, props_bar_visible || tags_n > 0);
}

// ---------- PRIVATE FUNCTIONS ---------- //

static GtkWidget *errands_task_tag_new(ErrandsTask *self, const char *tag) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append(GTK_BOX(box), g_object_new(GTK_TYPE_IMAGE, "icon-name", "errands-tag-symbolic", NULL));
  GtkWidget *label = gtk_label_new(tag);
  g_object_set(label, "max-width-chars", 15, "halign", GTK_ALIGN_START, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_box_append(GTK_BOX(box), label);
  GtkWidget *button = g_object_new(GTK_TYPE_BUTTON, "child", box, NULL);
  gtk_widget_add_css_class(button, "caption-heading");
  gtk_widget_add_css_class(button, "tag");
  g_signal_connect_swapped(button, "clicked", G_CALLBACK(errands_task_list_tags_dialog_show), self);

  return button;
}

// ---------- CALLBACKS ---------- //

static void on_complete_btn_toggle_cb(ErrandsTask *self, GtkCheckButton *btn) {
  LOG("Toggle completion '%s'", errands_data_get_str(self->data->data, DATA_PROP_UID));
  bool active = gtk_check_button_get_active(btn);
  icaltimetype now = icaltime_get_date_time_now();
  errands_data_set(self->data->data, DATA_PROP_COMPLETED_TIME, active ? now : icaltime_null_time());
  // Complete all sub-tasks if completed
  if (active) {
    g_autoptr(GPtrArray) sub_tasks = g_ptr_array_new();
    errands_task_data_get_flat_list(self->data, sub_tasks);
    for_range(i, 0, sub_tasks->len) {
      TaskData *sub_task = g_ptr_array_index(sub_tasks, i);
      errands_data_set(sub_task->data, DATA_PROP_COMPLETED_TIME, now);
    }
  }
  // Uncomplete parents tasks if unchecked
  else {
    TaskData *parent = self->data->parent;
    while (parent) {
      errands_data_set(parent->data, DATA_PROP_COMPLETED_TIME, icaltime_null_time());
      parent = parent->parent;
    }
  }
  errands_data_write_list(self->data->list);
  errands_list_data_sort(self->data->list);
  errands_task_list_reload(state.main_window->task_list, true);
  // Update task list
  errands_task_list_update_title(state.main_window->task_list);
  errands_sidebar_update_filter_rows(state.main_window->sidebar);
  errands_sidebar_task_list_row_update_counter(
      errands_sidebar_task_list_row_get(errands_data_get_str(self->data->data, DATA_PROP_LIST_UID)));
  // Sync
  errands_sync_schedule_list(self->data->list);
}

static void on_title_edit_cb(GtkEditableLabel *label, GParamSpec *pspec, gpointer user_data) {
  bool editing = gtk_editable_label_get_editing(label);
  LOG("Task: Edit '%s'", editing ? "on" : "off");
  ErrandsTask *task = user_data;
  const char *curr_text = errands_data_get_str(task->data->data, DATA_PROP_TEXT);
  if (editing) {
    gtk_widget_set_visible(GTK_WIDGET(label), true);
    gtk_editable_set_text(GTK_EDITABLE(task->edit_title), curr_text);
    gtk_widget_grab_focus(task->edit_title);
  } else {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(task->edit_title));
    if (!text || STR_EQUAL("", text)) {
      gtk_widget_set_visible(task->title, true);
      return;
    }
    if (STR_EQUAL(text, curr_text)) {
      gtk_widget_set_visible(task->title, true);
      return;
    }
    gtk_widget_set_visible(task->title, true);
    errands_data_set(task->data->data, DATA_PROP_TEXT, text);
    errands_data_write_list(task->data->list);
    gtk_label_set_label(GTK_LABEL(task->title), text);
    errands_sync_schedule_task(task->data);
  }
}

static void on_unpin_btn_clicked_cb(ErrandsTask *self, GtkToggleButton *btn) {
  errands_data_set_and_write(self->data->data, DATA_PROP_PINNED, false, self->data->list);
  errands_sidebar_update_filter_rows(state.main_window->sidebar);
  errands_task_list_reload(state.main_window->task_list, true);
}

static void on_sub_task_entry_activated(GtkEntry *entry, ErrandsTask *self) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (STR_EQUAL(text, "")) return;
  TaskData *new_data = errands_task_data_create_task(self->data->list, self->data, text);
  g_ptr_array_add(self->data->children, new_data);
  errands_data_write_list(self->data->list);
  errands_task_data_sort_sub_tasks(self->data);
  // Reset text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  LOG("Task '%s': Add sub-task '%s'", errands_data_get_str(new_data->data, DATA_PROP_UID),
      errands_data_get_str(new_data->data, DATA_PROP_UID));
  errands_sync_schedule_task(self->data);
  errands_task_list_reload(state.main_window->task_list, true);
  gtk_widget_grab_focus(GTK_WIDGET(entry));
  errands_sidebar_update_filter_rows(state.main_window->sidebar);
}

static void on_expand_toggle_cb(ErrandsTask *self, GtkGestureClick *ctrl, gint n_press, gdouble x, gdouble y) {
  bool new_expanded = !errands_data_get_bool(self->data->data, DATA_PROP_EXPANDED);
  LOG("Task '%s': Toggle expand: %d", errands_data_get_str(self->data->data, DATA_PROP_UID), new_expanded);
  errands_data_set(self->data->data, DATA_PROP_EXPANDED, new_expanded);
  errands_data_write_list(self->data->list);
  errands_task_list_reload(state.main_window->task_list, true);
  gtk_widget_grab_focus(GTK_WIDGET(self->sub_entry));
}

// --- DND CALLBACKS --- //

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
