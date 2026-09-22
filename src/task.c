#include "task.h"
#include "adwaita.h"
#include "config.h"
#include "data.h"
#include "sidebar.h"
#include "state.h"
#include "task-item.h"
#include "task-list.h"
#include "task-properties-dialog.h"
#include "utils.h"
#include "window.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

static GtkWidget *errands_task_tag_new(const char *tag);

static void on_edit_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_copy_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_color_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_notes_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_priority_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_attachments_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_tags_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_date_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_cancel_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_delete_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_export_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);
static void on_priority_action_cb(GSimpleAction *action, GVariant *value, ErrandsTask *self);

// Callbacks
static void on_title_edit_cb(GtkEditableLabel *label, GParamSpec *pspec, gpointer user_data);
static void on_sub_task_entry_activated_cb(GtkEntry *entry, ErrandsTask *self);
static void on_drop_motion_ctrl_enter_cb(ErrandsTask *self);

static GdkContentProvider *on_drag_prepare_cb(GtkDragSource *source, double x, double y, ErrandsTask *self);
static void on_drag_begin_cb(GtkDragSource *source, GdkDrag *drag, ErrandsTask *self);
static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTask *self);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsTask, errands_task, GTK_TYPE_BOX)

enum {
  PROP_0,

  PROP_ITEM,
  PROP_COLOR,
  PROP_PRIORITY,
  PROP_NOTES,
  PROP_DTSTART,
  PROP_DTEND,
  PROP_TAGS,
  PROP_ATTACHMENTS,
  PROP_RRULE,

  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsTask *self = ERRANDS_TASK(object);
  switch (prop_id) {
  case PROP_ITEM: g_value_set_object(value, self->item); break;
  case PROP_COLOR: g_value_set_string(value, self->color); break;
  case PROP_PRIORITY: g_value_set_int(value, self->priority); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsTask *self = ERRANDS_TASK(object);
  switch (prop_id) {
  case PROP_ITEM: self->item = g_value_get_object(value); break;
  case PROP_COLOR: {
    self->color = g_value_get_string(value);
    gtk_widget_set_color(GTK_WIDGET(self), self->color);
  } break;
  case PROP_PRIORITY: {
    self->priority = g_value_get_int(value);
    // Update action state
    GSimpleAction *priority_action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(self->ag), "priority"));
    GVariant *value = g_variant_new_string(errands_task_item_get_priority_as_string(self->item));
    g_simple_action_set_state(priority_action, value);
    // UI
    gtk_label_set_label(GTK_LABEL(self->priority_label), errands_task_item_get_priority_as_tstring(self->item));
    gtk_widget_set_visible(self->priority_box, self->priority > 0);

    errands_task_update_toolbar(self);

    //  const uint8_t priority = adw_spin_row_get_value(self->custom_row);
    // if (errands_data_get_priority(data->ical) != priority) {
    //   changed = true;
    //   errands_data_set_priority(data->ical, priority);
    //   switch (state.main_window->task_list->page) {
    //   case ERRANDS_TASK_LIST_PAGE_ALL:
    //   case ERRANDS_TASK_LIST_PAGE_TODAY: errands_data_sort(); break;
    //   case ERRANDS_TASK_LIST_PAGE_TASK_LIST: errands_list_data_sort(data->list); break;
    //   }
    // }
  } break;
  case PROP_NOTES: {
    self->notes = g_value_get_string(value);
    gtk_widget_set_visible(self->notes_btn, self->notes != NULL);
    errands_task_update_toolbar(self);
  } break;
  case PROP_DTSTART: {
    icaltimetype *dt = g_value_get_pointer(value);
    self->dtstart = *dt;
    bool is_null = icaltime_is_null_date(self->dtstart);
    gtk_widget_set_visible(self->dtstart_btn, !is_null);
    errands_task_update_toolbar(self);
    if (is_null) break;

    g_autofree gchar *date_str = NULL;
    g_autoptr(GDateTime) sdt = g_date_time_new_from_unix_local(icaltime_as_timet(self->dtstart));
    if (!self->dtstart.is_date) date_str = g_date_time_format(sdt, "%d %b %H:%M");
    else date_str = g_date_time_format(sdt, "%d %b");
    const char *label = tmp_str_printf("%s: %s", C_("Start date", "Start"), date_str);
    adw_button_content_set_label(ADW_BUTTON_CONTENT(self->dtstart_btn_content), label);
  } break;
  case PROP_DTEND: {
    icaltimetype *dt = g_value_get_pointer(value);
    self->dtend = *dt;
    bool has_dt = !icaltime_is_null_date(self->dtend);
    g_autofree gchar *rrule = errands_task_item_get_rrule_as_string(self->item);
    gtk_widget_set_visible(self->dtend_btn, has_dt && !rrule);
    errands_task_update_toolbar(self);
    if (!has_dt) break;

    g_autofree gchar *date_str = NULL;
    g_autoptr(GDateTime) sdt = g_date_time_new_from_unix_local(icaltime_as_timet(self->dtend));
    if (!self->dtend.is_date) date_str = g_date_time_format(sdt, "%d %b %H:%M");
    else date_str = g_date_time_format(sdt, "%d %b");
    const char *label = tmp_str_printf("%s: %s", C_("Due date", "Due"), date_str);
    adw_button_content_set_label(ADW_BUTTON_CONTENT(self->dtend_btn_content), label);
    gtk_widget_remove_css_class(self->dtend_btn, "error");
    if (errands_task_item_is_due(self->item)) gtk_widget_add_css_class(self->dtend_btn, "error");
  } break;
  case PROP_TAGS: {
    self->tags = g_value_get_pointer(value);
    adw_wrap_box_remove_all(ADW_WRAP_BOX(self->tags_box));
    const guint tags_n = self->tags ? g_strv_length(self->tags) : 0;
    for_range(i, 0, tags_n) adw_wrap_box_append(ADW_WRAP_BOX(self->tags_box), errands_task_tag_new(self->tags[i]));
    errands_task_update_toolbar(self);
  } break;
  case PROP_ATTACHMENTS: {
    self->attachments = g_value_get_pointer(value);
    bool has_attachments = self->attachments && g_strv_length(self->attachments) > 0;
    gtk_widget_set_visible(self->attachments_btn, has_attachments);
    errands_task_update_toolbar(self);
    if (!has_attachments) break;
    const char *label = tmp_str_printf("%ud", g_strv_length(self->attachments));
    adw_button_content_set_label(ADW_BUTTON_CONTENT(self->attachments_btn_content), label);
  } break;
  case PROP_RRULE: {
    self->rrule = g_value_get_pointer(value);
    g_autofree gchar *label = errands_task_item_get_rrule_as_string(self->item);
    if (label) gtk_widget_set_visible(self->dtend_btn, false);
    gtk_widget_set_visible(self->rrule_btn, label != NULL);
    errands_task_update_toolbar(self);
    if (label) adw_button_content_set_label(ADW_BUTTON_CONTENT(self->rrule_btn_content), label);
  } break;

  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void errands_task_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK);
  G_OBJECT_CLASS(errands_task_parent_class)->dispose(gobject);
}

static void errands_task_class_init(ErrandsTaskClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = errands_task_dispose;

  object_class->get_property = get_property;
  object_class->set_property = set_property;

  obj_properties[PROP_ITEM] = g_param_spec_object("item", "Task Item", "Task item associated with the task.",
                                                  ERRANDS_TYPE_TASK_ITEM, G_PARAM_READWRITE);
  obj_properties[PROP_COLOR] = g_param_spec_string("color", "Color", "Color of the task.", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_PRIORITY] =
      g_param_spec_int("priority", "Priority", "Priority of the task.", 0, 10, 0, G_PARAM_READWRITE);
  obj_properties[PROP_NOTES] = g_param_spec_string("notes", "Notes", "Notes of the task.", NULL, G_PARAM_WRITABLE);
  obj_properties[PROP_DTSTART] =
      g_param_spec_pointer("dtstart", "Start Date", "Start date of the task.", G_PARAM_WRITABLE);
  obj_properties[PROP_DTEND] = g_param_spec_pointer("dtend", "End Date", "End date of the task.", G_PARAM_WRITABLE);
  obj_properties[PROP_TAGS] = g_param_spec_pointer("tags", "Tags", "Tags of the task.", G_PARAM_WRITABLE);
  obj_properties[PROP_ATTACHMENTS] =
      g_param_spec_pointer("attachments", "Attachments", "Attachments of the task.", G_PARAM_WRITABLE);
  obj_properties[PROP_RRULE] = g_param_spec_pointer("rrule", "RRule", "RRule of the task.", G_PARAM_WRITABLE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);

  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), RESOURCE_PATH "/ui/task.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, complete_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, title);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, edit_title);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, popover_menu);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, toolbar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, priority_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, priority_label);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, tags_box);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, dtstart_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, dtstart_btn_content);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, dtend_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, dtend_btn_content);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, rrule_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, rrule_btn_content);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, notes_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, attachments_btn);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, attachments_btn_content);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, sub_entry);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ErrandsTask, drop_motion_ctrl);

  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_title_edit_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_sub_task_entry_activated_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_drop_motion_ctrl_enter_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_drag_prepare_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_drag_begin_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_drop_cb);
}

static void errands_task_init(ErrandsTask *self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  self->ag = errands_add_action_group(self, "task");
  errands_add_action(self->ag, "edit", on_edit_action_cb, self, NULL);
  errands_add_action(self->ag, "copy", on_copy_action_cb, self, NULL);
  errands_add_action(self->ag, "color", on_color_action_cb, self, NULL);
  errands_add_action(self->ag, "notes", on_notes_action_cb, self, NULL);
  errands_add_action(self->ag, "attachments", on_attachments_action_cb, self, NULL);
  errands_add_action(self->ag, "tags", on_tags_action_cb, self, NULL);
  errands_add_action(self->ag, "date", on_date_action_cb, self, NULL);
  errands_add_action(self->ag, "cancel", on_cancel_action_cb, self, NULL);
  errands_add_action(self->ag, "delete", on_delete_action_cb, self, NULL);
  errands_add_action(self->ag, "export", on_export_action_cb, self, NULL);
  errands_add_stateful_action(self->ag, "priority", G_VARIANT_TYPE_STRING, g_variant_new_string("none"),
                              on_priority_action_cb, self);
}

ErrandsTask *errands_task_new() { return g_object_new(ERRANDS_TYPE_TASK, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_update_toolbar(ErrandsTask *self) {
  bool has_dtstart = !icaltime_is_null_date(self->dtstart);
  bool has_dtend = !icaltime_is_null_date(self->dtend);
  bool has_tags = self->tags && g_strv_length(self->tags) > 0;
  bool has_attachments = self->attachments && g_strv_length(self->attachments) > 0;
  bool has_priority = self->priority != 0;
  bool has_notes = self->notes != NULL;
  bool has_rrule = self->rrule != NULL;

  bool visible = has_dtstart || has_dtend || has_tags || has_attachments || has_priority || has_notes || has_rrule;
  gtk_widget_set_visible(self->toolbar, visible);
}

// ---------- PRIVATE FUNCTIONS ---------- //

static GtkWidget *errands_task_tag_new(const char *tag) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append(GTK_BOX(box), g_object_new(GTK_TYPE_IMAGE, "icon-name", "errands-tag-symbolic", NULL));
  GtkWidget *label = g_object_new(GTK_TYPE_LABEL, "label", tag, "max-width-chars", 15, "halign", GTK_ALIGN_START,
                                  "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_box_append(GTK_BOX(box), label);
  GtkWidget *button = g_object_new(GTK_TYPE_BUTTON, "child", box, "action-name", "task.tags", NULL);
  gtk_widget_add_css_class(button, "caption-heading");
  gtk_widget_add_css_class(button, "tag");

  return button;
}

// ---------- ACTION CALLBACKS ---------- //

static void on_edit_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  gtk_popover_popdown(GTK_POPOVER(self->popover_menu));
  gtk_editable_label_start_editing(GTK_EDITABLE_LABEL(self->edit_title));
}

static void on_copy_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  gtk_popover_popdown(GTK_POPOVER(self->popover_menu));
  const char *text = errands_task_item_get_title(self->item);
  GdkClipboard *clipboard = gdk_display_get_clipboard(gtk_widget_get_display(GTK_WIDGET(self)));
  gdk_clipboard_set(clipboard, G_TYPE_STRING, text);
  errands_window_add_toast(_("Copied to Clipboard"), 1);
}

static void on_notes_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  errands_task_properties_dialog_show(ERRANDS_TASK_PROPERTY_DIALOG_PAGE_NOTES, self->item);
}

static void on_priority_action_cb(GSimpleAction *action, GVariant *value, ErrandsTask *self) {
  g_simple_action_set_state(action, value);
  errands_task_item_set_priority_from_string(self->item, g_variant_get_string(value, NULL));
}

static void on_attachments_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  errands_task_properties_dialog_show(ERRANDS_TASK_PROPERTY_DIALOG_PAGE_ATTACHMENTS, self->item);
}

static void on_tags_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  errands_task_properties_dialog_show(ERRANDS_TASK_PROPERTY_DIALOG_PAGE_TAGS, self->item);
}

static void on_date_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  errands_task_properties_dialog_show(ERRANDS_TASK_PROPERTY_DIALOG_PAGE_DATE, self->item);
}

static void __get_children_tree_list_rows(GtkTreeListRow *parent, GPtrArray *array) {
  size_t i = 0;
  GtkTreeListRow *child_row = gtk_tree_list_row_get_child_row(parent, i);
  while (child_row) {
    g_ptr_array_add(array, child_row);
    __get_children_tree_list_rows(child_row, array);
    child_row = gtk_tree_list_row_get_child_row(parent, ++i);
  }
}

static void __get_parents_tree_list_rows(GtkTreeListRow *child, GPtrArray *array) {
  GtkTreeListRow *parent_row = gtk_tree_list_row_get_parent(child);
  while (parent_row) {
    g_ptr_array_add(array, parent_row);
    parent_row = gtk_tree_list_row_get_parent(parent_row);
  }
}

static void on_cancel_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  errands_task_item_set_cancelled(self->item, !errands_task_item_get_cancelled(self->item));
}

static void on_delete_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  errands_task_item_delete(self->item);
  // errands_task_list_item_remove_deleted_tasks();

  // errands_data_set_deleted(self->data->ical, true);
  // errands_sync_delete_task(self->data);
  // g_autoptr(GPtrArray) children = g_ptr_array_sized_new(self->data->children->len);
  // errands_task_data_get_flat_list(self->data, children);
  // for_range(i, 0, children->len) {
  //   TaskData *child = g_ptr_array_index(children, i);
  //   errands_data_set_deleted(child->ical, true);
  //   errands_sync_delete_task(child);
  // }
  // errands_list_data_save(self->data->list);

  // GListStore *parent_model = NULL;
  // ErrandsTaskItem *parent = errands_task_item_get_parent(self->item);
  // if (parent) {
  //   parent_model = G_LIST_STORE(errands_task_item_get_children_model(parent));
  //   ErrandsTask *parent_task = NULL;
  //   g_object_get(G_OBJECT(parent), "task-widget", &parent_task, NULL);
  //   // if (parent_task && GTK_IS_WIDGET(parent_task)) errands_task_update_progress(parent_task);
  // } else parent_model = state.main_window->task_list->all_tasks_model;
  // guint pos;
  // if (g_list_store_find(parent_model, self->item, &pos)) g_list_store_remove(parent_model, pos);
  // // g_object_notify(G_OBJECT(parent), "children-model-is-empty");

  errands_sidebar_update_filter_rows();
  errands_sidebar_task_list_update_counter(errands_task_item_get_uid(errands_task_item_get_parent(self->item)));
  errands_task_list_update(state.main_window->task_list);
}

static void on_export_finish_cb(GObject *obj, GAsyncResult *res, ErrandsTaskItem *item) {
  g_autoptr(GFile) f = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!f) return;
  g_autofree char *path = g_file_get_path(f);
  if (!errands_task_item_export(item, path)) {
    errands_window_add_toast(_("Failed to Export"), 2);
    return;
  }
  errands_window_add_toast(_("Exported"), 1);
}

static void on_export_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  const char *filename = tmp_str_printf("%s.ics", errands_task_item_get_uid(self->item));
  g_object_set(dialog, "initial-name", filename, NULL);
  gtk_file_dialog_save(dialog, GTK_WINDOW(state.main_window), NULL, (GAsyncReadyCallback)on_export_finish_cb,
                       self->item);
}

static void on_finish_cb(GObject *source_object, GAsyncResult *res, gpointer data) {
  ErrandsTaskItem *item = data;
  g_autoptr(GdkRGBA) rgba = gtk_color_dialog_choose_rgba_finish(GTK_COLOR_DIALOG(source_object), res, NULL);
  if (!rgba) return;
  char hex_string[8];
  gdk_rgba_to_hex_string(rgba, hex_string);
  errands_task_item_set_color(item, hex_string);
}

static void on_color_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  g_autoptr(GtkColorDialog) color_dialog = g_object_new(GTK_TYPE_COLOR_DIALOG, "with-alpha", false, NULL);
  const char *color = errands_task_item_get_color(self->item);
  GdkRGBA rgba = {1};
  if (color) gdk_rgba_parse(&rgba, color);
  gtk_color_dialog_choose_rgba(color_dialog, GTK_WINDOW(state.main_window), &rgba, NULL, on_finish_cb, self->item);
}

// ---------- CALLBACKS ---------- //

static void on_title_edit_cb(GtkEditableLabel *label, GParamSpec *pspec, gpointer user_data) {
  bool editing = gtk_editable_label_get_editing(label);
  ErrandsTask *self = user_data;
  const char *curr_text = errands_task_item_get_title(self->item);
  if (editing) {
    gtk_widget_set_visible(GTK_WIDGET(label), true);
    gtk_editable_set_text(GTK_EDITABLE(self->edit_title), curr_text);
    gtk_widget_grab_focus(self->edit_title);
  } else {
    const char *text = gtk_editable_get_text(GTK_EDITABLE(self->edit_title));
    if (!text || STR_EQUAL("", text)) {
      gtk_widget_set_visible(self->title, true);
      return;
    }
    if (STR_EQUAL(text, curr_text)) {
      gtk_widget_set_visible(self->title, true);
      return;
    }
    gtk_widget_set_visible(self->title, true);
    g_autofree gchar *markup = str_to_markup(text);
    g_object_set(self->item, "title", markup, NULL);
  }
}

static void on_sub_task_entry_activated_cb(GtkEntry *entry, ErrandsTask *self) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
  if (STR_EQUAL(text, "")) return;
  ErrandsTaskItem *task = (ErrandsTaskItem *)errands_task_item_create_task(self->item, text);
  // g_list_store_append(state.main_window->task_list->all_tasks_model, task);
  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_widget_get_ancestor(GTK_WIDGET(self), GTK_TYPE_TREE_EXPANDER));
  gtk_tree_list_row_set_expanded(gtk_tree_expander_get_list_row(expander), true);
  // Reset text
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  errands_sidebar_update_filter_rows();
}

// --- DND CALLBACKS --- //

static guint on_drop_motion_ctrl_enter_timeout_cb(ErrandsTask *self) {
  if (!self || !GTK_IS_WIDGET(self)) return G_SOURCE_REMOVE;

  GtkTreeExpander *expander = GTK_TREE_EXPANDER(gtk_widget_get_ancestor(GTK_WIDGET(self), GTK_TYPE_TREE_EXPANDER));
  GtkTreeListRow *row = gtk_tree_expander_get_list_row(expander);
  bool expanded = gtk_tree_list_row_get_expanded(row);
  if (!expanded && gtk_drop_controller_motion_contains_pointer(self->drop_motion_ctrl))
    gtk_tree_list_row_set_expanded(row, true);

  return G_SOURCE_REMOVE;
}

static void on_drop_motion_ctrl_enter_cb(ErrandsTask *self) {
  g_timeout_add_once(500, (GSourceOnceFunc)on_drop_motion_ctrl_enter_timeout_cb, self);
}

static GdkContentProvider *on_drag_prepare_cb(GtkDragSource *source, double x, double y, ErrandsTask *task) {
  GValue value = G_VALUE_INIT;
  g_value_init(&value, ERRANDS_TYPE_TASK_ITEM);
  g_value_set_object(&value, task->item);

  return gdk_content_provider_new_for_value(&value);
}

static void on_drag_begin_cb(GtkDragSource *source, GdkDrag *drag, ErrandsTask *task) {
  const char *text = errands_task_item_get_title(task->item);
  g_autofree gchar *label;
  if (strlen(text) > 20) {
    g_autofree gchar *truncated = g_strndup(text, 17);
    label = g_strconcat(truncated, "...", NULL);
  } else label = g_strdup(text);
  g_object_set(gtk_drag_icon_get_for_drag(drag), "child", g_object_new(GTK_TYPE_BUTTON, "label", label, NULL), NULL);
}

// static bool __task_data_is_sub_task_of(TaskData *data, TaskData *possible_parent) {
//   g_autoptr(GPtrArray) subs = g_ptr_array_new();
//   errands_task_data_get_flat_list(possible_parent, subs);
//   return g_ptr_array_find(subs, data, NULL);
// }

static gboolean on_drop_cb(GtkDropTarget *target, const GValue *value, double x, double y, ErrandsTask *task) {
  //   // Check items
  //   ErrandsTaskItem *drop_item = g_value_get_object(value);
  //   ErrandsTaskItem *tgt_item = task->item;
  //   if (!tgt_item || !drop_item || tgt_item == drop_item) return false;
  //   TaskData *drop_data = errands_task_item_get_data(drop_item);
  //   TaskData *tgt_data = errands_task_item_get_data(tgt_item);
  //   if (__task_data_is_sub_task_of(tgt_data, drop_data)) {
  //     errands_window_add_toast(_("Can't add task as a child of itself"), 2);
  //     return false;
  //   }

  //   bool changing_list = tgt_data->list != drop_data->list;

  //   ListData *drop_data_old_list = drop_data->list;

  //   // Get old parent task
  //   ErrandsTask *old_parent_task = NULL;
  //   if (drop_data->parent) {
  //     ErrandsTaskItem *drop_item_parent = errands_task_item_get_parent(drop_item);
  //     if (drop_item_parent) g_object_get(drop_item_parent, "task-widget", &old_parent_task, NULL);
  //   }

  //   // Move task data
  //   bool moved = errands_task_data_move_to_list(drop_data, tgt_data->list, tgt_data);
  //   if (!moved) return false;

  //   errands_list_data_save(tgt_data->list);
  //   if (changing_list) {
  //     errands_list_data_save(drop_data_old_list);
  //     errands_sidebar_task_list_update_counter(errands_data_get_uid(drop_data_old_list->ical));
  //     errands_task_list_update_title(state.main_window->task_list);
  //   }
  //   errands_sidebar_task_list_update_counter(errands_data_get_uid(tgt_data->list->ical));

  //   // Add child to target model
  //   GListStore *tgt_children_model = G_LIST_STORE(errands_task_item_get_children_model(tgt_item));
  //   g_list_store_append(tgt_children_model, drop_item);

  //   // Remove from parent model
  //   ErrandsTaskItem *drop_parent_item = errands_task_item_get_parent(drop_item);
  //   if (drop_parent_item) {
  //     GListStore *drop_parent_model = G_LIST_STORE(errands_task_item_get_children_model(drop_parent_item));
  //     guint idx;
  //     if (drop_parent_model && g_list_store_find(drop_parent_model, drop_item, &idx))
  //       g_list_store_remove(drop_parent_model, idx);
  //   } else {
  //     errands_task_list_filter_toplevel(state.main_window->task_list, GTK_FILTER_CHANGE_DIFFERENT);
  //   }
  //   errands_task_item_set_parent(drop_item, tgt_item);

  //   // Uncomplete and uncancel
  //   if (errands_data_is_completed(tgt_data->ical) && !errands_data_is_completed(drop_data->ical))
  //     gtk_widget_activate_action(GTK_WIDGET(task), "task.complete", NULL, NULL);
  //   if (errands_data_get_cancelled(tgt_data->ical) && !errands_data_get_cancelled(drop_data->ical))
  //     gtk_widget_activate_action(GTK_WIDGET(task), "task.cancel", NULL, NULL);

  //   // Update progress
  //   errands_task_item_update(tgt_item);
  //   if (!changing_list && old_parent_task) errands_task_item_update(old_parent_task->item);

  return true;
}
