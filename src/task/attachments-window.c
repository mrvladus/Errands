#include "attachments-window.h"
#include "../data.h"
#include "../state.h"

#include <glib/gi18n.h>

// ------------------------------------------------------ //
//                  ATTACHMENTS WINDOW                    //
// ------------------------------------------------------ //

static void errands_attachments_window_update_ui();
static void on_errands_attachments_window_close(ErrandsAttachmentsWindow *win);
static void on_errands_attachments_window_attachment_add(GtkButton *btn);
static void on_errands_attachments_window_row_activate(GtkListBox *box,
                                                       ErrandsAttachmentsWindowRow *row);

G_DEFINE_TYPE(ErrandsAttachmentsWindow, errands_attachments_window, ADW_TYPE_DIALOG)

static void errands_attachments_window_class_init(ErrandsAttachmentsWindowClass *class) {}

static void errands_attachments_window_init(ErrandsAttachmentsWindow *self) {
  g_object_set(self, "title", _("Attachments"), "content-width", 330, "content-height", 400, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_attachments_window_close), NULL);

  // Header bar
  GtkWidget *hb = adw_header_bar_new();
  self->title = adw_window_title_new(_("Attachments"), "");
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(hb), self->title);

  // Add button
  GtkWidget *add_btn = gtk_button_new_from_icon_name("errands-add-symbolic");
  g_signal_connect(add_btn, "clicked", G_CALLBACK(on_errands_attachments_window_attachment_add),
                   NULL);
  adw_header_bar_pack_start(ADW_HEADER_BAR(hb), add_btn);

  // Vertical box
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  g_object_set(scrl, "propagate-natural-height", true, NULL);
  gtk_box_append(GTK_BOX(vbox), scrl);

  // List box
  self->list_box = gtk_list_box_new();
  g_object_set(self->list_box, "selection-mode", GTK_SELECTION_NONE, "margin-start", 12,
               "margin-end", 12, "margin-top", 6, "margin-bottom", 12, "valign", GTK_ALIGN_START,
               "vexpand", false, NULL);
  gtk_widget_add_css_class(self->list_box, "boxed-list");
  g_signal_connect(self->list_box, "row-activated",
                   G_CALLBACK(on_errands_attachments_window_row_activate), NULL);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl), self->list_box);

  // Placeholder
  self->placeholder = adw_status_page_new();
  g_object_set(self->placeholder, "icon-name", "errands-attachment-symbolic", "title",
               _("No Attachments"), "description", _("Click \"+\" button to add new attachment"),
               "vexpand", true, NULL);
  g_object_bind_property(self->placeholder, "visible", scrl, "visible",
                         G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_widget_add_css_class(self->placeholder, "compact");
  gtk_box_append(GTK_BOX(vbox), self->placeholder);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), hb);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), vbox);
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsAttachmentsWindow *errands_attachments_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_ATTACHMENTS_WINDOW, NULL));
}

void errands_attachments_window_show(ErrandsTask *task) {
  // Set task
  state.attachments_window->task = task;
  // Remove rows
  gtk_list_box_remove_all(GTK_LIST_BOX(state.attachments_window->list_box));
  // Add rows
  for (int i = 0; i < task->data->attachments->len; i++) {
    const char *path = task->data->attachments->pdata[i];
    ErrandsAttachmentsWindowRow *row =
        errands_attachments_window_row_new(g_file_new_for_path(path));
    gtk_list_box_append(GTK_LIST_BOX(state.attachments_window->list_box), GTK_WIDGET(row));
  }
  errands_attachments_window_update_ui();
  // Show dialog
  adw_dialog_present(ADW_DIALOG(state.attachments_window), GTK_WIDGET(state.main_window));
}

static void errands_attachments_window_update_ui() {
  if (state.attachments_window->task->data->attachments->len > 0) {
    char *len = g_strdup_printf("%d", state.attachments_window->task->data->attachments->len);
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.attachments_window->title), len);
    g_free(len);
    gtk_widget_set_visible(state.attachments_window->placeholder, false);
  } else {
    adw_window_title_set_subtitle(ADW_WINDOW_TITLE(state.attachments_window->title), "");
    gtk_widget_set_visible(state.attachments_window->placeholder, true);
  }
}

// --- SIGNAL HANDLERS FOR ATTACHMENTS WINDOW --- //

static void on_errands_attachments_window_close(ErrandsAttachmentsWindow *win) {
  // Add css class to button if attachments not empty
  GPtrArray *attachments = win->task->data->attachments;
  gtk_widget_remove_css_class(win->task->toolbar->attachments_btn, "accent");
  if (attachments->len > 0)
    gtk_widget_add_css_class(win->task->toolbar->attachments_btn, "accent");
}

static void __on_open_finish(GObject *obj, GAsyncResult *res, gpointer data) {
  GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(obj), res, NULL);
  if (!file)
    return;

  char *path = g_file_get_path(file);
  GPtrArray *attachments = state.attachments_window->task->data->attachments;
  for (int i = 0; i < attachments->len; i++) {
    if (!strcmp(attachments->pdata[i], path)) {
      g_free(path);
      return;
    }
  }
  ErrandsAttachmentsWindowRow *row = errands_attachments_window_row_new(file);
  gtk_list_box_append(GTK_LIST_BOX(state.attachments_window->list_box), GTK_WIDGET(row));

  g_ptr_array_add(attachments, path);
  errands_data_write();

  errands_attachments_window_update_ui();
}

static void on_errands_attachments_window_attachment_add(GtkButton *btn) {
  GtkFileDialog *dialog = gtk_file_dialog_new();
  gtk_file_dialog_open(dialog, GTK_WINDOW(state.main_window), NULL, __on_open_finish, NULL);
}

static void on_errands_attachments_window_row_activate(GtkListBox *box,
                                                       ErrandsAttachmentsWindowRow *row) {
  GtkFileLauncher *l = gtk_file_launcher_new(row->file);
  gtk_file_launcher_launch(l, GTK_WINDOW(state.main_window), NULL, NULL, NULL);
  g_object_unref(l);
}

// ------------------------------------------------------------- //
//               ATTACHMENTS WINDOW ATTACHMENT ROW               //
// ------------------------------------------------------------- //

static void on_errands_attachments_window_row_delete(GtkButton *btn,
                                                     ErrandsAttachmentsWindowRow *row);
static void on_errands_attachments_window_row_destroy(ErrandsAttachmentsWindowRow *row);

G_DEFINE_TYPE(ErrandsAttachmentsWindowRow, errands_attachments_window_row, ADW_TYPE_ACTION_ROW)

static void errands_attachments_window_row_class_init(ErrandsAttachmentsWindowRowClass *class) {}

static void errands_attachments_window_row_init(ErrandsAttachmentsWindowRow *self) {
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(self), true);

  // Delete button
  self->del_btn = gtk_button_new_from_icon_name("errands-trash-symbolic");
  g_object_set(self->del_btn, "valign", GTK_ALIGN_CENTER, NULL);
  g_signal_connect(self->del_btn, "clicked", G_CALLBACK(on_errands_attachments_window_row_delete),
                   self);
  gtk_widget_add_css_class(self->del_btn, "flat");
  gtk_widget_add_css_class(self->del_btn, "error");
  adw_action_row_add_suffix(ADW_ACTION_ROW(self), self->del_btn);
}

ErrandsAttachmentsWindowRow *errands_attachments_window_row_new(GFile *file) {
  ErrandsAttachmentsWindowRow *row = g_object_new(ERRANDS_TYPE_ATTACHMENTS_WINDOW_ROW, NULL);
  g_signal_connect(row, "destroy", G_CALLBACK(on_errands_attachments_window_row_destroy), NULL);

  row->file = file;
  char *path = g_file_get_path(file);
  char *basename = g_file_get_basename(file);
  adw_action_row_set_subtitle(ADW_ACTION_ROW(row), path);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), basename);

  g_free(path);
  g_free(basename);

  return row;
}

// --- SIGNAL HANDLERS FOR TAG ROW --- //

static void on_errands_attachments_window_row_delete(GtkButton *btn,
                                                     ErrandsAttachmentsWindowRow *row) {
  char *path = g_file_get_path(row->file);
  GPtrArray *attachments = state.attachments_window->task->data->attachments;
  for (int i = 0; i < attachments->len; i++)
    if (!strcmp(attachments->pdata[i], path)) {
      char *item = g_ptr_array_steal_index(attachments, i);
      g_free(item);
    }
  g_free(path);
  errands_data_write();
  gtk_list_box_remove(GTK_LIST_BOX(state.attachments_window->list_box), GTK_WIDGET(row));
  errands_attachments_window_update_ui();
}

static void on_errands_attachments_window_row_destroy(ErrandsAttachmentsWindowRow *row) {
  g_free(row->file);
}
