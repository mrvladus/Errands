// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "window.h"
#include "config.h"
#include "gtk/gtk.h"
#include "settings.h"
#include "state.h"
#include "task-item.h"
#include "task-list.h"
#include "utils.h"

#include <adwaita.h>
#include <glib.h>
#include <glib/gi18n.h>

#define ATTACHMENTS_LIST_BOX                                                                                                     \
  gtk_widget_get_first_child(gtk_widget_get_last_child(gtk_widget_get_first_child(GTK_WIDGET(self->attachments))))
#define TAGS_LIST_BOX gtk_widget_get_first_child(gtk_widget_get_last_child(gtk_widget_get_first_child(GTK_WIDGET(self->tags))))

static void on_add_attachment_action_cb(GSimpleAction *action, GVariant *param, ErrandsWindow *self);
static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, ErrandsWindow *self);
static void on_show_properties_toggled_cb(ErrandsWindow *self);
static void on_tag_entry_activated_cb(ErrandsWindow *self, AdwEntryRow *entry);

// ---------- WIDGET TEMPLATE ---------- //

G_DEFINE_TYPE(ErrandsWindow, errands_window, ADW_TYPE_APPLICATION_WINDOW)

static void errands_window_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_WINDOW);
  G_OBJECT_CLASS(errands_window_parent_class)->dispose(gobject);
}

static void errands_window_class_init(ErrandsWindowClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_window_dispose;
  g_type_ensure(ERRANDS_TYPE_SIDEBAR);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST);
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/window.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, toast_overlay);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, split_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, sidebar);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, task_list);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, overlay_split_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, stack);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, notes_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, notes_buffer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, start_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, due_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, rrule_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, attachments);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsWindow, tags);

  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_show_properties_toggled_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_tag_entry_activated_cb);
}

static void errands_window_init(ErrandsWindow *self) {
  g_message("Window: Create");
  gtk_widget_init_template(GTK_WIDGET(self));

  GSimpleActionGroup *ag = errands_add_action_group(self, "window");
  errands_add_action(ag, "add-attachment", on_add_attachment_action_cb, self, NULL);

  // Set theme
  AdwStyleManager *style_manager = adw_style_manager_get_default();
  g_object_set(style_manager, "color-scheme", errands_settings_get_theme(state.settings), NULL);
  g_object_bind_property(state.settings, "theme", style_manager, "color-scheme", G_BINDING_DEFAULT);

  // Notes
  gtk_source_init();
  GtkSourceLanguageManager *lang_mgr = gtk_source_language_manager_get_default();
  gtk_source_buffer_set_language(self->notes_buffer, gtk_source_language_manager_get_language(lang_mgr, "markdown"));
  AdwStyleManager *style_mgr = adw_style_manager_get_default();
  const char *theme = adw_style_manager_get_dark(style_mgr) ? "Adwaita-dark" : "Adwaita";
  GtkSourceStyleSchemeManager *style_scheme_mgr = gtk_source_style_scheme_manager_get_default();
  GtkSourceStyleScheme *style_scheme = gtk_source_style_scheme_manager_get_scheme(style_scheme_mgr, theme);
  gtk_source_buffer_set_style_scheme(self->notes_buffer, style_scheme);
  g_object_bind_property_full(style_mgr, "dark", self->notes_buffer, "style-scheme", G_BINDING_SYNC_CREATE,
                              (GBindingTransformFunc)on_style_toggled_cb, NULL, self, NULL);

  g_message("Window: Created");
}

// ---------- PRIVATE FUNCTIONS ---------- //

static void on_tag_delete_cb(GtkButton *btn, AdwActionRow *row);

static void on_attachment_clicked_cb(GtkListBox *box, AdwActionRow *attachment) {
  g_autoptr(GFile) file = g_file_new_for_path(adw_action_row_get_subtitle(attachment));
  g_autoptr(GtkFileLauncher) l = gtk_file_launcher_new(file);
  gtk_file_launcher_launch(l, GTK_WINDOW(state.main_window), NULL, NULL, NULL);
}

static void on_attachment_delete_cb(GtkButton *btn, AdwActionRow *attachment) {
  ErrandsWindow *self = (ErrandsWindow *)gtk_widget_get_ancestor(GTK_WIDGET(btn), ERRANDS_TYPE_WINDOW);
  adw_preferences_group_remove(self->attachments, GTK_WIDGET(attachment));
  GStrv cur_attachments = errands_task_item_get_attachments(self->item);
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for (size_t i = 0; i < g_strv_length(cur_attachments); i++) {
    if (!g_str_equal(cur_attachments[i], adw_action_row_get_subtitle(attachment)))
      g_strv_builder_add(builder, cur_attachments[i]);
  }
  GStrv attachments = g_strv_builder_end(builder);
  errands_task_item_set_attachments(self->item, attachments);
  gtk_widget_set_visible(GTK_WIDGET(self->attachments), attachments && g_strv_length(attachments) > 0);
}

static inline GtkWidget *attachment_new(const char *path) {
  GtkWidget *attachment = adw_action_row_new();
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(attachment), true);
  GtkWidget *btn = gtk_button_new_from_icon_name("errands-trash-symbolic");
  gtk_widget_set_tooltip_text(btn, _("Remove Attachment"));
  gtk_widget_add_css_class(btn, "error");
  gtk_widget_add_css_class(btn, "flat");
  gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
  g_signal_connect(btn, "clicked", G_CALLBACK(on_attachment_delete_cb), attachment);
  adw_action_row_add_suffix(ADW_ACTION_ROW(attachment), btn);
  g_autofree gchar *basename = g_path_get_basename(path);
  adw_action_row_set_subtitle(ADW_ACTION_ROW(attachment), path);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(attachment), basename);
  g_signal_connect(attachment, "activated", G_CALLBACK(on_attachment_clicked_cb), attachment);

  return attachment;
}

static inline void add_attachment(ErrandsWindow *self, const char *path) {
  GStrv cur_attachments = errands_task_item_get_attachments(self->item);
  if (cur_attachments && g_strv_contains((const gchar *const *)cur_attachments, path)) return;
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  if (cur_attachments) g_strv_builder_addv(builder, (const char **)cur_attachments);
  g_strv_builder_add(builder, path);
  GStrv attachments = g_strv_builder_end(builder);
  errands_task_item_set_attachments(self->item, attachments);
  GtkWidget *attachment = attachment_new(path);
  adw_preferences_group_add(self->attachments, attachment);
  gtk_widget_set_visible(GTK_WIDGET(self->attachments), true);
}

static GtkWidget *tag_new(const char *tag) {
  GtkWidget *row = adw_action_row_new();

  GtkWidget *check_btn = gtk_check_button_new();
  gtk_widget_set_tooltip_text(check_btn, _("Enable Tag"));
  adw_action_row_add_prefix(ADW_ACTION_ROW(row), check_btn);
  adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), check_btn);

  GtkWidget *delete_btn = gtk_button_new_from_icon_name("errands-trash-symbolic");
  gtk_widget_set_tooltip_text(delete_btn, _("Remove Tag"));
  gtk_widget_add_css_class(delete_btn, "error");
  gtk_widget_add_css_class(delete_btn, "flat");
  gtk_widget_set_valign(delete_btn, GTK_ALIGN_CENTER);
  g_signal_connect(delete_btn, "clicked", G_CALLBACK(on_tag_delete_cb), row);
  adw_action_row_add_suffix(ADW_ACTION_ROW(row), delete_btn);

  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), tag);

  return row;
}

static void add_tag(ErrandsWindow *self, const char *tag) {
  GtkWidget *row = tag_new(tag);
  GStrv tags = errands_task_item_get_tags(self->item);
  const bool has_tag = tags && g_strv_contains((const gchar *const *)tags, tag);
  GtkWidget *check_btn = adw_action_row_get_activatable_widget(ADW_ACTION_ROW(row));
  gtk_check_button_set_active(GTK_CHECK_BUTTON(check_btn), has_tag);
  gtk_list_box_append(GTK_LIST_BOX(TAGS_LIST_BOX), GTK_WIDGET(row));
}

static void reload_tags(ErrandsWindow *self) {
  g_autoptr(GPtrArray) tag_rows = get_children(TAGS_LIST_BOX);
  for_range(i, 0, tag_rows->len) {
    GtkWidget *child = g_ptr_array_index(tag_rows, i);
    adw_preferences_group_remove(self->tags, child);
  }
  GStrv tags = errands_settings_get_tags(state.settings);
  guint len = tags ? g_strv_length(tags) : 0;
  for_range(i, 0, len) add_tag(self, tags[i]);
  gtk_widget_set_visible(GTK_WIDGET(self->tags), len > 0);
}

static void on_tag_delete_cb(GtkButton *btn, AdwActionRow *row) {
  const char *tag = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row));
  errands_settings_remove_tag(state.settings, tag);
  ErrandsWindow *self = (ErrandsWindow *)gtk_widget_get_ancestor(GTK_WIDGET(btn), ERRANDS_TYPE_WINDOW);
  reload_tags(self);
}

static void set_item(ErrandsWindow *self, ErrandsTaskItem *item) {
  self->item = item;
  adw_overlay_split_view_set_show_sidebar(self->overlay_split_view, true);

  // Date
  errands_date_chooser_reset(self->start_date_chooser);
  errands_date_chooser_set_dt(self->start_date_chooser, errands_task_item_get_dtstart(item));
  errands_date_chooser_reset(self->due_date_chooser);
  errands_date_chooser_set_dt(self->due_date_chooser, errands_task_item_get_dtend(item));
  errands_rrule_row_reset(self->rrule_row);
  ICalRecurrence *rrule = errands_task_item_get_rrule(item);
  errands_rrule_row_set_rrule(self->rrule_row, rrule);
  adw_expander_row_set_expanded(ADW_EXPANDER_ROW(self->rrule_row),
                                rrule && i_cal_recurrence_get_freq(rrule) != I_CAL_NO_RECURRENCE);
  g_clear_pointer(&rrule, i_cal_recurrence_unref);

  // Notes
  const char *notes = errands_task_item_get_notes(item);
  gtk_text_buffer_set_text(GTK_TEXT_BUFFER(self->notes_buffer), notes ? notes : "", -1);

  // Attachments
  g_autoptr(GPtrArray) rows = get_children(ATTACHMENTS_LIST_BOX);
  for_range(i, 0, rows->len) {
    GtkWidget *child = g_ptr_array_index(rows, i);
    adw_preferences_group_remove(self->attachments, child);
  }
  GStrv attachments = errands_task_item_get_attachments(item);
  guint len = attachments ? g_strv_length(attachments) : 0;
  for_range(i, 0, len) adw_preferences_group_add(self->attachments, attachment_new(attachments[i]));
  gtk_widget_set_visible(GTK_WIDGET(self->attachments), len > 0);

  // Tags
  reload_tags(self);
}

// ---------- PUBLIC FUNCTIONS ---------- //

ErrandsWindow *errands_window_new(GtkApplication *app) {
  ErrandsWindow *win =
      g_object_new(ERRANDS_TYPE_WINDOW, "application", app, "default-width", errands_settings_get_window_width(state.settings),
                   "default-height", errands_settings_get_window_height(state.settings), "maximized",
                   errands_settings_get_maximized(state.settings), NULL);
  g_object_bind_property(state.settings, "maximized", win, "maximized", G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "window-width", win, "default-width", G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "window-height", win, "default-height", G_BINDING_DEFAULT | G_BINDING_BIDIRECTIONAL);
  return win;
}

void errands_window_add_toast(const char *msg, int timeout) {
  g_message("Window: Add Toast '%s'", msg);
  AdwToast *toast = adw_toast_new(msg);
  adw_toast_set_timeout(toast, timeout);
  adw_toast_overlay_add_toast(state.main_window->toast_overlay, toast);
}

void errands_window_show_properties(ErrandsWindow *self, ErrandsTaskItem *item) {
  g_message("Window: Show properties");
  set_item(self, item);
  adw_view_stack_set_visible_child_name(self->stack, "properties");
}

void errands_window_show_notes(ErrandsWindow *self, ErrandsTaskItem *item) {
  g_message("Window: Show notes");
  set_item(self, item);
  adw_view_stack_set_visible_child_name(self->stack, "notes");
}

// ---------- ACTIONS ---------- //

static void on_add_attachment_action_finish_cb(GObject *obj, GAsyncResult *res, gpointer data) {
  g_autoptr(GFile) file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!file) return;
  ErrandsWindow *self = data;
  GFileInfo *info = g_file_query_info(file, "xattr::document-portal.host-path", G_FILE_QUERY_INFO_NONE, NULL, NULL);
  const char *real_path = g_file_info_get_attribute_as_string(info, "xattr::document-portal.host-path");
  g_autofree char *path = g_file_get_path(file);
  add_attachment(self, real_path ? real_path : path);
}

static void on_add_attachment_action_cb(GSimpleAction *action, GVariant *param, ErrandsWindow *self) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  gtk_file_dialog_open(dialog, GTK_WINDOW(self), NULL, on_add_attachment_action_finish_cb, self);
}

// ---------- CALLBACKS ---------- //

static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, ErrandsWindow *self) {
  if (!self) return false;
  GtkSourceStyleSchemeManager *style_scheme_mgr = gtk_source_style_scheme_manager_get_default();
  const char *theme = g_value_get_boolean(from_value) ? "Adwaita-dark" : "Adwaita";
  GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(style_scheme_mgr, theme);
  gtk_source_buffer_set_style_scheme(self->notes_buffer, scheme);

  return false;
}

static void on_show_properties_toggled_cb(ErrandsWindow *self) {
  gboolean show = adw_overlay_split_view_get_show_sidebar(self->overlay_split_view);
  if (show) return;
  g_message("Window: Close properties");

  // Date
  ICalTime *new_sdt = errands_date_chooser_get_dt(self->start_date_chooser);
  errands_task_item_set_dtstart(self->item, new_sdt);
  ICalTime *new_ddt = errands_date_chooser_get_dt(self->due_date_chooser);
  errands_task_item_set_dtend(self->item, new_ddt);
  if (errands_task_item_is_due(self->item)) errands_sidebar_update_filter_rows();

  // Set rrule
  ICalRecurrence *new_rrule = NULL;
  if (adw_expander_row_get_expanded(ADW_EXPANDER_ROW(self->rrule_row))) new_rrule = errands_rrule_row_get_rrule(self->rrule_row);
  errands_task_item_set_rrule(self->item, new_rrule);
  if (i_cal_time_is_null_date(new_sdt) && new_rrule && i_cal_recurrence_get_freq(new_rrule) != I_CAL_NO_RECURRENCE)
    errands_task_item_set_dtstart(self->item, i_cal_time_get_date_time_now());
  g_clear_pointer(&new_rrule, i_cal_recurrence_unref);

  // Notes
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(self->notes_buffer), &start);
  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(self->notes_buffer), &end);
  g_autofree char *new_notes = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(self->notes_buffer), &start, &end, FALSE);
  errands_task_item_set_notes(self->item, new_notes);

  // Tags
  g_autoptr(GPtrArray) tag_rows = get_children(TAGS_LIST_BOX);
  GStrv old_tags = errands_task_item_get_tags(self->item);
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for_range(i, 0, tag_rows->len) {
    AdwActionRow *row = g_ptr_array_index(tag_rows, i);
    const char *tag = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row));
    bool active = gtk_check_button_get_active(GTK_CHECK_BUTTON(adw_action_row_get_activatable_widget(row)));
    bool exists = old_tags && g_strv_contains((const gchar *const *)old_tags, tag);
    if (active && !exists) g_strv_builder_add(builder, tag);
  }
  GStrv new_tags = g_strv_builder_end(builder);
  if (g_strv_hash(old_tags) != g_strv_hash(new_tags)) errands_task_item_set_tags(self->item, new_tags);
}

static void on_tag_entry_activated_cb(ErrandsWindow *self, AdwEntryRow *entry) {
  const char *tag = string_trim((char *)gtk_editable_get_text(GTK_EDITABLE(entry)));
  if (g_str_equal(tag, "")) return;
  GStrv tags = errands_settings_get_tags(state.settings);
  for (size_t i = 0; i < g_strv_length(tags); i++)
    if (g_str_equal(tag, tags[i])) return;
  errands_settings_add_tag(state.settings, tag);
  add_tag(self, tag);
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  gtk_widget_set_visible(GTK_WIDGET(self->tags), true);
}
