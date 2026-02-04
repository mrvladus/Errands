#include "task-properties-dialog.h"
#include "data.h"
#include "date-chooser.h"
#include "notifications.h"
#include "settings.h"
#include "state.h"
#include "sync.h"
#include "task-list.h"
#include "utils.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

static void on_add_attachment_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self);

static void on_dialog_close_cb(ErrandsTaskPropertiesDialog *self);
static gboolean on_style_toggled_cb(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data);

static void on_priority_toggled_cb(ErrandsTaskPropertiesDialog *self, GtkCheckButton *btn);

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
static char *page_names[] = {"date", "notes", "priority", "attachments", "tags"};

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
  // Priority
  AdwActionRow *high_row;
  AdwActionRow *medium_row;
  AdwActionRow *low_row;
  AdwActionRow *none_row;
  AdwSpinRow *custom_row;
  // Attachments
  AdwPreferencesGroup *attachments;
  // Tags
  AdwPreferencesGroup *tags;

  ErrandsTask *task;
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
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/task-properties-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, tbv);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, title);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, stack);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, start_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, due_date_chooser);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, rrule_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, notes_view);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, notes_buffer);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, high_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, medium_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, low_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, none_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, custom_row);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, attachments);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsTaskPropertiesDialog, tags);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_dialog_close_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_priority_toggled_cb);
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

void errands_task_properties_dialog_show(ErrandsTaskPropertiesDialogPage page, ErrandsTask *task) {
  if (!self) self = errands_task_properties_dialog_new();
  self->task = task;
  int page_n = CLAMP(page, 0, ERRANDS_TASK_PROPERTY_DIALOG_N_PAGES - 1);
  adw_view_stack_set_visible_child_name(self->stack, page_names[page_n]);

  // Title
  gtk_label_set_label(self->title, errands_data_get_text(task->data->ical));

  // Date
  errands_date_chooser_reset(self->start_date_chooser);
  errands_date_chooser_reset(self->due_date_chooser);
  errands_task_list_date_dialog_rrule_row_reset(self->rrule_row);
  errands_date_chooser_set_dt(self->start_date_chooser, errands_data_get_start(task->data->ical));
  errands_date_chooser_set_dt(self->due_date_chooser, errands_data_get_due(task->data->ical));
  struct icalrecurrencetype rrule = errands_data_get_rrule(task->data->ical);
  errands_task_list_date_dialog_rrule_row_set_rrule(self->rrule_row, rrule);
  adw_expander_row_set_expanded(ADW_EXPANDER_ROW(self->rrule_row), rrule.freq != ICAL_NO_RECURRENCE);

  // Notes
  const char *notes = errands_data_get_notes(task->data->ical);
  if (notes) {
    g_autofree gchar *text = gtk_source_utils_unescape_search_text(notes);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->notes_view)), text, -1);
  } else gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->notes_view)), "", -1);
  if (page_n == ERRANDS_TASK_PROPERTY_DIALOG_PAGE_NOTES) gtk_widget_grab_focus(GTK_WIDGET(self->notes_view));

  // Priority
  const int priority = errands_data_get_priority(task->data->ical);
  if (priority == 0) adw_action_row_activate(self->none_row);
  else if (priority == 1) adw_action_row_activate(self->low_row);
  else if (priority > 1 && priority < 6) adw_action_row_activate(self->medium_row);
  else if (priority > 5) adw_action_row_activate(self->high_row);

  // Attachments
  gtk_list_box_remove_all(GTK_LIST_BOX(ATTACHMENTS_LIST_BOX));
  g_auto(GStrv) attachments = errands_data_get_attachments(task->data->ical);
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
  g_auto(GStrv) cur_attachments = errands_data_get_attachments(self->task->data->ical);
  // If already contains - return
  if (cur_attachments && g_strv_contains((const gchar *const *)cur_attachments, path)) return;
  // Add attachment
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  if (cur_attachments) g_strv_builder_addv(builder, (const char **)cur_attachments);
  g_strv_builder_add(builder, path);
  g_auto(GStrv) attachments = g_strv_builder_end(builder);
  errands_data_set_attachments(self->task->data->ical, attachments);
  errands_list_data_save(self->task->data->list);
  GtkWidget *attachment = errands_task_properties_dialog_attachment_new(path);
  gtk_list_box_append(GTK_LIST_BOX(ATTACHMENTS_LIST_BOX), attachment);
  gtk_widget_set_visible(GTK_WIDGET(self->attachments), true);
  errands_sync_update_task(self->task->data);
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
  g_auto(GStrv) tags = errands_data_get_tags(self->task->data->ical);
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

static void on_add_attachment_action_cb(GSimpleAction *action, GVariant *param, ErrandsTask *self) {
  g_autoptr(GtkFileDialog) dialog = gtk_file_dialog_new();
  gtk_file_dialog_open(dialog, GTK_WINDOW(state.main_window), NULL, __on_attachment_open_finish, NULL);
}

// ---------- CALLBACKS ---------- //

static void on_dialog_close_cb(ErrandsTaskPropertiesDialog *self) {
  TaskData *data = self->task->data;
  bool changed = false;

  // Date
  icaltimetype curr_sdt = errands_data_get_start(data->ical);
  icaltimetype new_sdt = errands_date_chooser_get_dt(self->start_date_chooser);
  if (!icaltime_is_null_time(new_sdt) && icaltime_compare(curr_sdt, new_sdt)) {
    errands_data_set_start(data->ical, new_sdt);
    changed = true;
  }
  bool rrule_is_set = adw_expander_row_get_expanded(ADW_EXPANDER_ROW(self->rrule_row));
  icaltimetype curr_ddt = errands_data_get_due(data->ical);
  icaltimetype new_ddt = errands_date_chooser_get_dt(self->due_date_chooser);
  if (!rrule_is_set && !icaltime_is_null_time(new_ddt) && icaltime_compare(curr_ddt, new_ddt)) {
    errands_data_set_due(data->ical, new_ddt);
    changed = true;
  }
  // Set rrule
  struct icalrecurrencetype old_rrule = ICALRECURRENCETYPE_INITIALIZER;
  struct icalrecurrencetype new_rrule = ICALRECURRENCETYPE_INITIALIZER;
  // Get old and new rrule
  icalproperty *rrule_prop = icalcomponent_get_first_property(data->ical, ICAL_RRULE_PROPERTY);
  if (rrule_prop) old_rrule = icalproperty_get_rrule(rrule_prop);
  if (adw_expander_row_get_expanded(ADW_EXPANDER_ROW(self->rrule_row)))
    new_rrule = errands_task_list_date_dialog_rrule_row_get_rrule(self->rrule_row);
  // Compare them and set / remove
  if (!icalrecurrencetype_compare(&new_rrule, &old_rrule)) {
    if (rrule_prop) {
      // Delete rrule if new rrule is not set
      if (new_rrule.freq == ICAL_NO_RECURRENCE) icalcomponent_remove_property(data->ical, rrule_prop);
      // Set new rrule
      else icalproperty_set_rrule(rrule_prop, new_rrule);
    } else {
      // Set new rrule
      if (new_rrule.freq != ICAL_NO_RECURRENCE)
        icalcomponent_add_property(data->ical, icalproperty_new_rrule(new_rrule));
    }
    changed = true;
  }

  // Notes
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(self->notes_buffer), &start);
  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(self->notes_buffer), &end);
  g_autofree char *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(self->notes_buffer), &start, &end, FALSE);
  const char *notes = errands_data_get_notes(data->ical);
  if (text && (!notes || !g_str_equal(text, notes))) {
    errands_data_set_notes(data->ical, text);
    changed = true;
  }

  // Priority
  const uint8_t priority = adw_spin_row_get_value(self->custom_row);
  if (errands_data_get_priority(data->ical) != priority) {
    changed = true;
    errands_data_set_priority(data->ical, priority);
    switch (state.main_window->task_list->page) {
    case ERRANDS_TASK_LIST_PAGE_ALL:
    case ERRANDS_TASK_LIST_PAGE_TODAY: errands_data_sort(); break;
    case ERRANDS_TASK_LIST_PAGE_TASK_LIST: errands_list_data_sort(data->list); break;
    case ERRANDS_TASK_LIST_PAGE_PINNED: break;
    }
  }

  // Tags
  g_autoptr(GPtrArray) tag_rows = get_children(TAGS_LIST_BOX);
  g_auto(GStrv) old_tags = errands_data_get_tags(data->ical);
  for_range(i, 0, tag_rows->len) {
    AdwActionRow *row = g_ptr_array_index(tag_rows, i);
    const char *tag = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row));
    bool active = gtk_check_button_get_active(GTK_CHECK_BUTTON(adw_action_row_get_activatable_widget(row)));
    bool exists = g_strv_contains((const gchar *const *)old_tags, tag);
    if (active && !exists) {
      errands_data_add_tag(data->ical, tag);
      changed = true;
    } else if (!active && exists) {
      errands_data_remove_tag(data->ical, tag);
      changed = true;
    }
  }

  // Save if changed
  if (changed) {
    if (!icaltime_is_null_time(errands_data_get_due(data->ical))) {
      errands_data_set_notified(data->ical, false);
      errands_notifications_add(data);
    }
    if (self->task->data->parent) errands_task_data_sort_sub_tasks(self->task->data->parent);
    else errands_data_sort();
    errands_list_data_save(data->list);
    errands_task_update_toolbar(self->task);
    errands_sync_update_task(data);
    errands_task_list_reload(state.main_window->task_list, true);
    errands_sidebar_update_filter_rows();
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

// --- PRIORITY --- //

static void on_priority_toggled_cb(ErrandsTaskPropertiesDialog *self, GtkCheckButton *btn) {
  if (!gtk_check_button_get_active(btn)) return;
  const char *name = gtk_widget_get_name(GTK_WIDGET(btn));
  uint8_t val = 0;
  if (g_str_equal(name, "none")) val = 0;
  else if (g_str_equal(name, "low")) val = 1;
  else if (g_str_equal(name, "medium")) val = 5;
  else if (g_str_equal(name, "high")) val = 9;
  adw_spin_row_set_value(self->custom_row, val);
}

// --- ATTACHMENTS --- //

static void on_attachment_clicked_cb(GtkListBox *box, AdwActionRow *attachment) {
  g_autoptr(GFile) file = g_file_new_for_path(adw_action_row_get_subtitle(attachment));
  g_autoptr(GtkFileLauncher) l = gtk_file_launcher_new(file);
  gtk_file_launcher_launch(l, GTK_WINDOW(state.main_window), NULL, NULL, NULL);
}

static void on_attachment_delete_cb(GtkButton *btn, AdwActionRow *attachment) {
  ErrandsTask *task = self->task;
  g_auto(GStrv) cur_attachments = errands_data_get_attachments(task->data->ical);
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for (size_t i = 0; i < g_strv_length(cur_attachments); i++)
    if (!g_str_equal(cur_attachments[i], adw_action_row_get_subtitle(attachment)))
      g_strv_builder_add(builder, cur_attachments[i]);
  g_auto(GStrv) attachments = g_strv_builder_end(builder);
  errands_data_set_attachments(task->data->ical, attachments);
  errands_list_data_save(task->data->list);
  gtk_list_box_remove(GTK_LIST_BOX(ATTACHMENTS_LIST_BOX), GTK_WIDGET(attachment));
  gtk_widget_set_visible(GTK_WIDGET(self->attachments), g_strv_length(attachments) > 0);
  errands_sync_update_task(task->data);
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
