#include "task-properties-dialog.h"
#include "data.h"
#include "date-chooser.h"
#include "glib.h"
#include "notifications.h"
#include "settings.h"
#include "state.h"
#include "sync.h"
#include "task-item.h"
#include "task-list.h"
#include "utils.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

static void on_add_attachment_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskPropertiesDialog *self);

static void on_dialog_close_cb(ErrandsTaskPropertiesDialog *self);
static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data);

#define ATTACHMENTS_LIST_BOX                                                                                           \
  gtk_widget_get_first_child(gtk_widget_get_last_child(gtk_widget_get_first_child(GTK_WIDGET(self->attachments))))
static GtkWidget *errands_task_properties_dialog_attachment_new(const char *path);
static void errands_task_properties_dialog_add_attachment(const char *path);
static void on_attachment_clicked_cb(GtkListBox *box, AdwActionRow *attachment);
static void on_attachment_delete_cb(GtkButton *btn, AdwActionRow *attachment);

#define TAGS_LIST_BOX                                                                                                  \
  gtk_widget_get_first_child(gtk_widget_get_last_child(gtk_widget_get_first_child(GTK_WIDGET(self->tags))))
static void errands_task_properties_dialog_add_tag(const char *tag);
static void on_tag_entry_activated_cb(AdwEntryRow *entry);
static void on_tag_delete_cb(GtkButton *btn, AdwActionRow *row);

static ErrandsTaskPropertiesDialog *self = NULL;
static char *page_names[] = {"date", "notes", "attachments", "tags"};

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsTaskPropertiesDialog {
  AdwDialog parent_instance;
  AdwToolbarView *tbv;
  GtkLabel *title;
  AdwViewStack *stack;
  // Date
  ErrandsDateChooser *start_date_chooser;
  ErrandsDateChooser *due_date_chooser;
  ErrandsTaskListDateDialogRruleRow *rrule_row;
  // Notes
  GtkSourceView *notes_view;
  GtkSourceBuffer *notes_buffer;
  // Attachments
  AdwPreferencesGroup *attachments;
  // Tags
  AdwPreferencesGroup *tags;

  ErrandsTaskItem *item;
};

G_DEFINE_TYPE(ErrandsTaskPropertiesDialog, errands_task_properties_dialog, ADW_TYPE_DIALOG)

static void errands_task_properties_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_TASK_PROPERTIES_DIALOG);
  G_OBJECT_CLASS(errands_task_properties_dialog_parent_class)->dispose(gobject);
}

static void errands_task_properties_dialog_class_init(ErrandsTaskPropertiesDialogClass *class) {
  g_type_ensure(ERRANDS_TYPE_DATE_CHOOSER);
  g_type_ensure(ERRANDS_TYPE_TASK_LIST_DATE_DIALOG_RRULE_ROW);

  G_OBJECT_CLASS(class)->dispose = errands_task_properties_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/task-properties-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, tbv);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, title);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, stack);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, start_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, due_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, rrule_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, notes_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, notes_buffer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, attachments);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, tags);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_tag_entry_activated_cb);
}

static void errands_task_properties_dialog_init(ErrandsTaskPropertiesDialog *dialog) {
  gtk_widget_init_template(GTK_WIDGET(dialog));
  GSimpleActionGroup *ag = errands_add_action_group(dialog, "task-properties");
  errands_add_action(ag, "add-attachment", on_add_attachment_action_cb, dialog, NULL);

  // Notes
  gtk_source_init();
  GtkSourceLanguageManager *lang_mgr = gtk_source_language_manager_get_default();
  gtk_source_buffer_set_language(dialog->notes_buffer, gtk_source_language_manager_get_language(lang_mgr, "markdown"));
  AdwStyleManager *style_mgr = adw_style_manager_get_default();
  const char *theme = adw_style_manager_get_dark(style_mgr) ? "Adwaita-dark" : "Adwaita";
  GtkSourceStyleSchemeManager *style_scheme_mgr = gtk_source_style_scheme_manager_get_default();
  GtkSourceStyleScheme *style_scheme = gtk_source_style_scheme_manager_get_scheme(style_scheme_mgr, theme);
  gtk_source_buffer_set_style_scheme(dialog->notes_buffer, style_scheme);
  g_object_bind_property_full(style_mgr, "dark", dialog->notes_buffer, "style-scheme", G_BINDING_SYNC_CREATE,
                              on_style_toggled_cb, NULL, NULL, NULL);
}

ErrandsTaskPropertiesDialog *errands_task_properties_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TASK_PROPERTIES_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_task_properties_dialog_show(ErrandsTaskPropertiesDialogPage page, ErrandsTaskItem *item) {
  LOG("Task Properties: Open");
  if (!self) self = errands_task_properties_dialog_new();
  self->item = item;
  int page_n = CLAMP(page, 0, ERRANDS_TASK_PROPERTY_DIALOG_N_PAGES - 1);
  adw_view_stack_set_visible_child_name(self->stack, page_names[page_n]);

  // Title
  const char *title = errands_task_item_get_title(item);
  gtk_label_set_label(self->title, title ? title : _("Task Properties"));

  // Date
  errands_date_chooser_reset(self->start_date_chooser);
  errands_date_chooser_reset(self->due_date_chooser);
  errands_task_list_date_dialog_rrule_row_reset(self->rrule_row);
  errands_date_chooser_set_dt(self->start_date_chooser, errands_task_item_get_dtstart(item));
  errands_date_chooser_set_dt(self->due_date_chooser, errands_task_item_get_dtend(item));

  struct icalrecurrencetype *rrule = errands_data_get_rrule(errands_task_item_get_data(item)->ical);
  errands_task_list_date_dialog_rrule_row_set_rrule(self->rrule_row, rrule);
  adw_expander_row_set_expanded(ADW_EXPANDER_ROW(self->rrule_row), rrule && rrule->freq != ICAL_NO_RECURRENCE);

  // Notes
  const char *notes = errands_task_item_get_notes(item);
  gtk_text_buffer_set_text(GTK_TEXT_BUFFER(self->notes_buffer), notes ? notes : "", -1);
  if (page_n == ERRANDS_TASK_PROPERTY_DIALOG_PAGE_NOTES) gtk_widget_grab_focus(GTK_WIDGET(self->notes_view));

  // Attachments
  gtk_list_box_remove_all(GTK_LIST_BOX(ATTACHMENTS_LIST_BOX));
  GStrv attachments = errands_task_item_get_attachments(item);
  if (attachments)
    for (size_t i = 0; i < g_strv_length(attachments); i++) {
      // TODO: Use add func?
      GtkWidget *attachment = errands_task_properties_dialog_attachment_new(attachments[i]);
      gtk_list_box_append(GTK_LIST_BOX(ATTACHMENTS_LIST_BOX), GTK_WIDGET(attachment));
    }
  gtk_widget_set_visible(GTK_WIDGET(self->attachments), attachments && g_strv_length(attachments) > 0);

  // Tags
  gtk_list_box_remove_all(GTK_LIST_BOX(TAGS_LIST_BOX));
  g_auto(GStrv) tags = errands_settings_get_tags();
  g_auto(GStrv) all_tags_no_dups = gstrv_remove_duplicates(tags);
  if (all_tags_no_dups)
    for (size_t i = 0; i < g_strv_length(all_tags_no_dups); i++)
      errands_task_properties_dialog_add_tag(all_tags_no_dups[i]);
  gtk_widget_set_visible(GTK_WIDGET(self->tags), all_tags_no_dups && g_strv_length(all_tags_no_dups) > 0);

  adw_dialog_present(ADW_DIALOG(self), GTK_WIDGET(state.main_window));
}

// ---------- PRIVATE FUNCTIONS ---------- //

// --- ATTACHMENTS --- //

static GtkWidget *errands_task_properties_dialog_attachment_new(const char *path) {
  GtkWidget *attachment = adw_action_row_new();
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

static void errands_task_properties_dialog_add_attachment(const char *path) {
  // Get current attachments
  GStrv cur_attachments = errands_task_item_get_attachments(self->item);
  // If already contains - return
  if (cur_attachments && g_strv_contains((const gchar *const *)cur_attachments, path)) return;
  // Add attachment
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  if (cur_attachments) g_strv_builder_addv(builder, (const char **)cur_attachments);
  g_strv_builder_add(builder, path);
  GStrv attachments = g_strv_builder_end(builder);
  errands_task_item_set_attachments(self->item, attachments);
  GtkWidget *attachment = errands_task_properties_dialog_attachment_new(path);
  gtk_list_box_append(GTK_LIST_BOX(ATTACHMENTS_LIST_BOX), attachment);
  gtk_widget_set_visible(GTK_WIDGET(self->attachments), true);
}

// --- TAGS --- //

static GtkWidget *errands_task_properties_dialog_tag_new(const char *tag) {
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

static void errands_task_properties_dialog_add_tag(const char *tag) {
  GtkWidget *row = errands_task_properties_dialog_tag_new(tag);
  GStrv tags = errands_task_item_get_tags(self->item);
  const bool has_tag = tags && g_strv_contains((const gchar *const *)tags, tag);
  GtkWidget *check_btn = adw_action_row_get_activatable_widget(ADW_ACTION_ROW(row));
  gtk_check_button_set_active(GTK_CHECK_BUTTON(check_btn), has_tag);
  gtk_list_box_append(GTK_LIST_BOX(TAGS_LIST_BOX), GTK_WIDGET(row));
}

// ---------- ACTIONS ---------- //

static void __on_attachment_open_finish(GObject *obj, GAsyncResult *res, gpointer data) {
  g_autoptr(GFile) file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!file) return;
  GFileInfo *info = g_file_query_info(file, "xattr::document-portal.host-path", G_FILE_QUERY_INFO_NONE, NULL, NULL);
  const char *real_path = g_file_info_get_attribute_as_string(info, "xattr::document-portal.host-path");
  g_autofree char *path = g_file_get_path(file);
  errands_task_properties_dialog_add_attachment(real_path ? real_path : path);
}

static void on_add_attachment_action_cb(GSimpleAction *action, GVariant *param, ErrandsTaskPropertiesDialog *self) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  gtk_file_dialog_open(dialog, GTK_WINDOW(state.main_window), NULL, __on_attachment_open_finish, NULL);
}

// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskPropertiesDialog *self) {
  LOG("Task Properties: Close");
  bool changed = false;

  // Date
  icaltimetype curr_sdt = errands_task_item_get_dtstart(self->item);
  icaltimetype new_sdt = errands_date_chooser_get_dt(self->start_date_chooser);
  if (!icaltime_is_null_time(new_sdt) && icaltime_compare(curr_sdt, new_sdt)) {
    errands_task_item_set_dtstart(self->item, new_sdt);
    changed = true;
  }
  bool rrule_is_set = adw_expander_row_get_expanded(ADW_EXPANDER_ROW(self->rrule_row));
  icaltimetype curr_ddt = errands_task_item_get_dtend(self->item);
  icaltimetype new_ddt = errands_date_chooser_get_dt(self->due_date_chooser);
  if (!rrule_is_set && !icaltime_is_null_time(new_ddt) && icaltime_compare(curr_ddt, new_ddt)) {
    errands_task_item_set_dtend(self->item, new_ddt);
    changed = true;
  }

  // Set rrule
  // struct icalrecurrencetype *new_rrule = icalrecurrencetype_new();
  // if (adw_expander_row_get_expanded(ADW_EXPANDER_ROW(self->rrule_row)))
  //   errands_task_list_date_dialog_rrule_row_get_rrule(self->rrule_row, new_rrule);
  // if (errands_data_set_rrule(data->ical, new_rrule)) changed = true;
  // icalrecurrencetype_unref(new_rrule);

  // Notes
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(self->notes_buffer), &start);
  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(self->notes_buffer), &end);
  g_autofree char *new_notes = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(self->notes_buffer), &start, &end, FALSE);
  errands_task_item_set_notes(self->item, new_notes);

  // Tags
  g_autoptr(GPtrArray) tag_rows = get_children(TAGS_LIST_BOX);
  GStrv old_tags = errands_task_item_get_tags(self->item);
  for_range(i, 0, tag_rows->len) {
    AdwActionRow *row = g_ptr_array_index(tag_rows, i);
    const char *tag = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row));
    bool active = gtk_check_button_get_active(GTK_CHECK_BUTTON(adw_action_row_get_activatable_widget(row)));
    bool exists = g_strv_contains((const gchar *const *)old_tags, tag);
    if (active && !exists) {
      errands_task_item_add_tag(self->item, tag);
      changed = true;
    } else if (!active && exists) {
      errands_task_item_remove_tag(self->item, tag);
      changed = true;
    }
  }

  // Save if changed
  if (changed) {
    if (!icaltime_is_null_time(errands_task_item_get_dtend(self->item))) {
      TaskData *data = errands_task_item_get_data(self->item);
      errands_data_set_notified(data->ical, false);
      errands_notifications_add(data);
    }
    errands_sidebar_update_filter_rows();
    // g_autofree gchar *rrule_label = errands_data_get_rrule_as_string(data->ical);
    // LOG_DEBUG("%s", rrule_label);
  }
}

// --- NOTES --- //

static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data) {
  if (!self) return false;
  GtkSourceStyleSchemeManager *style_scheme_mgr = gtk_source_style_scheme_manager_get_default();
  const char *theme = g_value_get_boolean(from_value) ? "Adwaita-dark" : "Adwaita";
  GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(style_scheme_mgr, theme);
  gtk_source_buffer_set_style_scheme(self->notes_buffer, scheme);

  return false;
}

// --- ATTACHMENTS --- //

static void on_attachment_clicked_cb(GtkListBox *box, AdwActionRow *attachment) {
  g_autoptr(GFile) file = g_file_new_for_path(adw_action_row_get_subtitle(attachment));
  g_autoptr(GtkFileLauncher) l = gtk_file_launcher_new(file);
  gtk_file_launcher_launch(l, GTK_WINDOW(state.main_window), NULL, NULL, NULL);
}

static void on_attachment_delete_cb(GtkButton *btn, AdwActionRow *attachment) {
  GStrv cur_attachments = errands_task_item_get_attachments(self->item);
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for (size_t i = 0; i < g_strv_length(cur_attachments); i++) {
    if (!g_str_equal(cur_attachments[i], adw_action_row_get_subtitle(attachment)))
      g_strv_builder_add(builder, cur_attachments[i]);
  }
  GStrv attachments = g_strv_builder_end(builder);
  errands_task_item_set_attachments(self->item, attachments);
  gtk_list_box_remove(GTK_LIST_BOX(ATTACHMENTS_LIST_BOX), GTK_WIDGET(attachment));
  gtk_widget_set_visible(GTK_WIDGET(self->attachments), g_strv_length(attachments) > 0);
}

// --- TAGS --- //

static void on_tag_entry_activated_cb(AdwEntryRow *entry) {
  const char *tag = string_trim((char *)gtk_editable_get_text(GTK_EDITABLE(entry)));
  if (g_str_equal(tag, "")) return;
  g_auto(GStrv) tags = errands_settings_get_tags();
  for (size_t i = 0; i < g_strv_length(tags); i++)
    if (g_str_equal(tag, tags[i])) return;
  errands_settings_add_tag(tag);
  errands_task_properties_dialog_add_tag(tag);
  gtk_editable_set_text(GTK_EDITABLE(entry), "");
  gtk_widget_set_visible(GTK_WIDGET(self->tags), true);
}

static void on_tag_delete_cb(GtkButton *btn, AdwActionRow *row) {
  const char *tag = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row));
  errands_settings_remove_tag(tag);
  for_range(i, 0, errands_data_lists->len) {
    ListData *list = g_ptr_array_index(errands_data_lists, i);
    g_autoptr(GPtrArray) tasks = g_ptr_array_sized_new(list->children->len);
    errands_list_data_get_flat_list(list, tasks);
    for_range(j, 0, tasks->len) {
      TaskData *task = g_ptr_array_index(tasks, j);
      if (errands_data_remove_tag(task->ical, tag)) errands_sync_update_task(task);
    }
  }
  gtk_list_box_remove(GTK_LIST_BOX(TAGS_LIST_BOX), GTK_WIDGET(row));
  g_auto(GStrv) tags = errands_settings_get_tags();
  gtk_widget_set_visible(GTK_WIDGET(self->tags), tags && g_strv_length(tags) > 0);
}
