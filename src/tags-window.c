#include "tags-window.h"
#include "data.h"
#include "state.h"
#include "utils.h"

// ------------------------------------------------------ //
//                      TAGS WINDOW                       //
// ------------------------------------------------------ //

static void on_errands_tags_window_close(ErrandsTagsWindow *win);
static void on_errands_tags_window_tag_added(GtkEditable *entry,
                                             ErrandsTagsWindow *win);

G_DEFINE_TYPE(ErrandsTagsWindow, errands_tags_window, ADW_TYPE_DIALOG)

static void errands_tags_window_class_init(ErrandsTagsWindowClass *class) {}

static void errands_tags_window_init(ErrandsTagsWindow *self) {
  g_object_set(self, "title", "Tags", "content-width", 300, "content-height",
               400, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_tags_window_close),
                   NULL);

  // List box
  self->list_box = gtk_list_box_new();
  g_object_set(self->list_box, "selection-mode", GTK_SELECTION_NONE,
               "margin-start", 12, "margin-end", 12, "margin-top", 6,
               "margin-bottom", 24, "valign", GTK_ALIGN_START, "vexpand", false,
               NULL);
  gtk_widget_add_css_class(self->list_box, "boxed-list");

  // Entry
  self->entry = adw_entry_row_new();
  g_object_set(self->entry, "title", "Add new Tag", "margin-start", 12,
               "margin-end", 12, "margin-bottom", 3, "activatable", false,
               NULL);
  gtk_widget_add_css_class(self->entry, "card");
  g_signal_connect(self->entry, "entry-activated",
                   G_CALLBACK(on_errands_tags_window_tag_added), NULL);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  g_object_set(scrl, "child", self->list_box, NULL);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), adw_header_bar_new());
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), self->entry);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), scrl);
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsTagsWindow *errands_tags_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_TAGS_WINDOW, NULL));
}

void errands_tags_window_show(ErrandsTask *task) {
  state.tags_window->task = task;
  gtk_list_box_remove_all(GTK_LIST_BOX(state.tags_window->list_box));
  for (int i = 0; i < state.tags_data->len; i++) {
    ErrandsTagsWindowRow *row =
        errands_tags_window_row_new((const char *)state.tags_data->pdata[i]);
    gtk_list_box_append(GTK_LIST_BOX(state.tags_window->list_box),
                        GTK_WIDGET(row));
  }
  adw_dialog_present(ADW_DIALOG(state.tags_window),
                     GTK_WIDGET(state.main_window));
}

void errands_tags_window_add_tag(const char *tag) {}

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
  errands_data_tag_add((char *)text);
  errands_data_write();
  ErrandsTagsWindowRow *row = errands_tags_window_row_new(text);
  gtk_list_box_prepend(GTK_LIST_BOX(state.tags_window->list_box),
                       GTK_WIDGET(row));
  gtk_editable_set_text(entry, "");
}

// ------------------------------------------------------------- //
//                      TAGS WINDOW TAG ROW                      //
// ------------------------------------------------------------- //

static void on_errands_tags_window_row_toggle(GtkCheckButton *btn,
                                              ErrandsTagsWindowRow *row);

G_DEFINE_TYPE(ErrandsTagsWindowRow, errands_tags_window_row,
              ADW_TYPE_ACTION_ROW)

static void
errands_tags_window_row_class_init(ErrandsTagsWindowRowClass *class) {}

static void errands_tags_window_row_init(ErrandsTagsWindowRow *self) {
  self->toggle = gtk_check_button_new();
  adw_action_row_add_prefix(ADW_ACTION_ROW(self), self->toggle);
}

ErrandsTagsWindowRow *errands_tags_window_row_new(const char *tag) {
  ErrandsTagsWindowRow *row = g_object_new(ERRANDS_TYPE_TAGS_WINDOW_ROW, NULL);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), tag);
  g_signal_connect(row->toggle, "toggled",
                   G_CALLBACK(on_errands_tags_window_row_toggle), row);
  return row;
}

// --- SIGNAL HANDLERS FOR TAG ROW --- //

static void on_errands_tags_window_row_toggle(GtkCheckButton *btn,
                                              ErrandsTagsWindowRow *row) {}
