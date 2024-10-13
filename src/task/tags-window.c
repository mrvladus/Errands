#include "tags-window.h"
#include "../data.h"
#include "../state.h"
#include "../utils.h"

// ------------------------------------------------------ //
//                      TAGS WINDOW                       //
// ------------------------------------------------------ //

static void errands_tags_window_update_ui();
static void on_errands_tags_window_close(ErrandsTagsWindow *win);
static void on_errands_tags_window_tag_added(GtkEditable *entry,
                                             ErrandsTagsWindow *win);

G_DEFINE_TYPE(ErrandsTagsWindow, errands_tags_window, ADW_TYPE_DIALOG)

static void errands_tags_window_class_init(ErrandsTagsWindowClass *class) {}

static void errands_tags_window_init(ErrandsTagsWindow *self) {
  g_object_set(self, "title", "Tags", "content-width", 330, "content-height",
               400, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_tags_window_close),
                   NULL);

  // Header bar
  GtkWidget *hb = adw_header_bar_new();
  self->title = adw_window_title_new("Tags", "");
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), self->title);

  // Entry
  self->entry = adw_entry_row_new();
  g_object_set(self->entry, "title", "Add new Tag", "margin-start", 12,
               "margin-end", 12, "margin-bottom", 3, "activatable", false,
               NULL);
  gtk_widget_add_css_class(self->entry, "card");
  g_signal_connect(self->entry, "entry-activated",
                   G_CALLBACK(on_errands_tags_window_tag_added), self);

  // Vertical box
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  g_object_set(scrl, "propagate-natural-height", true, NULL);
  gtk_box_append(GTK_BOX(vbox), scrl);

  // List box
  self->list_box = gtk_list_box_new();
  g_object_set(self->list_box, "selection-mode", GTK_SELECTION_NONE,
               "margin-start", 12, "margin-end", 12, "margin-top", 6,
               "margin-bottom", 12, "valign", GTK_ALIGN_START, "vexpand", false,
               NULL);
  gtk_widget_add_css_class(self->list_box, "boxed-list");
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl), self->list_box);

  // Placeholder
  self->placeholder = adw_status_page_new();
  g_object_set(self->placeholder, "icon-name", "errands-tag-symbolic", "title",
               "No Tags", "description", "Add new ones in the entry above",
               "vexpand", true, NULL);
  g_object_bind_property(self->placeholder, "visible", scrl, "visible",
                         G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_widget_add_css_class(self->placeholder, "compact");
  gtk_box_append(GTK_BOX(vbox), self->placeholder);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), self->entry);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), vbox);
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsTagsWindow *errands_tags_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TAGS_WINDOW, NULL));
}

void errands_tags_window_show(ErrandsTask *task) {
  // Set task
  state.tags_window->task = task;
  // Remove rows
  gtk_list_box_remove_all(GTK_LIST_BOX(state.tags_window->list_box));
  errands_tags_window_update_ui();
  // Add rows
  for (int i = 0; i < state.tags_data->len; i++) {
    const char *tag = state.tags_data->pdata[i];
    ErrandsTagsWindowRow *row = errands_tags_window_row_new(tag);
    gtk_list_box_append(GTK_LIST_BOX(state.tags_window->list_box),
                        GTK_WIDGET(row));
  }
  // Show dialog
  adw_dialog_present(ADW_DIALOG(state.tags_window),
                     GTK_WIDGET(state.main_window));
}

static void errands_tags_window_update_ui() {
  if (state.tags_data->len > 0) {
    char *len = g_strdup_printf("%d", state.tags_data->len);
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.tags_window->title),
                                  len);
    g_free(len);
    gtk_widget_set_visible(state.tags_window->placeholder, false);
  } else {
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.tags_window->title),
                                  "");
    gtk_widget_set_visible(state.tags_window->placeholder, true);
  }
}

// --- SIGNAL HANDLERS FOR TAGS WINDOW --- //

static void on_errands_tags_window_close(ErrandsTagsWindow *win) {
  // TODO: update tags bar
}

static void on_errands_tags_window_tag_added(GtkEditable *entry,
                                             ErrandsTagsWindow *win) {
  const char *text = string_trim((char *)gtk_editable_get_text(entry));
  // Return if empty
  if (!strcmp(text, ""))
    return;
  // Return if duplicate
  for (int i = 0; i < state.tags_data->len; i++)
    if (!strcmp(text, state.tags_data->pdata[i]))
      return;
  errands_data_tag_add((char *)text);
  errands_data_write();
  ErrandsTagsWindowRow *row = errands_tags_window_row_new(text);
  gtk_list_box_prepend(GTK_LIST_BOX(state.tags_window->list_box),
                       GTK_WIDGET(row));
  gtk_editable_set_text(entry, "");
  errands_tags_window_update_ui();
}

// ------------------------------------------------------------- //
//                      TAGS WINDOW TAG ROW                      //
// ------------------------------------------------------------- //

static void on_errands_tags_window_row_toggle(GtkCheckButton *btn,
                                              ErrandsTagsWindowRow *row);
static void on_errands_tags_window_row_delete(GtkButton *btn,
                                              ErrandsTagsWindowRow *row);

G_DEFINE_TYPE(ErrandsTagsWindowRow, errands_tags_window_row,
              ADW_TYPE_ACTION_ROW)

static void
errands_tags_window_row_class_init(ErrandsTagsWindowRowClass *class) {}

static void errands_tags_window_row_init(ErrandsTagsWindowRow *self) {
  self->toggle = gtk_check_button_new();
  adw_action_row_add_prefix(ADW_ACTION_ROW(self), self->toggle);
  self->del_btn = gtk_button_new_from_icon_name("errands-trash-symbolic");
  g_object_set(self->del_btn, "valign", GTK_ALIGN_CENTER, NULL);
  g_signal_connect(self->del_btn, "clicked",
                   G_CALLBACK(on_errands_tags_window_row_delete), self);
  gtk_widget_add_css_class(self->del_btn, "flat");
  gtk_widget_add_css_class(self->del_btn, "error");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self), self->del_btn);
}

ErrandsTagsWindowRow *errands_tags_window_row_new(const char *tag) {
  ErrandsTagsWindowRow *row = g_object_new(ERRANDS_TYPE_TAGS_WINDOW_ROW, NULL);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), tag);
  bool has_tag =
      string_array_contains(state.tags_window->task->data->tags, tag);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(row->toggle), has_tag);
  g_signal_connect(row->toggle, "toggled",
                   G_CALLBACK(on_errands_tags_window_row_toggle), row);
  return row;
}

// --- SIGNAL HANDLERS FOR TAG ROW --- //

static void on_errands_tags_window_row_toggle(GtkCheckButton *btn,
                                              ErrandsTagsWindowRow *row) {
  bool active = gtk_check_button_get_active(btn);
  const char *text = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row));
  GPtrArray *task_tags = state.tags_window->task->data->tags;
  // If tag added
  if (active) {
    g_ptr_array_insert(task_tags, 0, strdup(text));
    errands_data_write();
  } else {
    for (int i = 0; i < task_tags->len; i++)
      if (!strcmp(text, task_tags->pdata[i])) {
        char *str = g_ptr_array_steal_index(task_tags, i);
        free(str);
        errands_data_write();
        break;
      }
  }
}

static void on_errands_tags_window_row_delete(GtkButton *btn,
                                              ErrandsTagsWindowRow *row) {
  const char *text = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(row));
  errands_data_tag_remove((char *)text);
  errands_data_write();
  gtk_list_box_remove(GTK_LIST_BOX(state.tags_window->list_box),
                      GTK_WIDGET(row));
  errands_tags_window_update_ui();
}
