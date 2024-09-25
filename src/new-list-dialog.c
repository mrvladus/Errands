#include "new-list-dialog.h"
#include "data.h"
#include "gtk/gtk.h"
#include "sidebar.h"
#include "state.h"
#include <string.h>

static void on_entry_changed_cb(AdwEntryRow *entry);
static void on_entry_activate_cb(AdwEntryRow *entry);
static void on_response_cb(AdwAlertDialog *dialog, gchar *response,
                           gpointer data);

// --- LOCAL STATE --- //

static AdwDialog *new_list_dialog;
static GtkWidget *new_list_dialog_entry;

// --- FUNCTIONS IMPLEMENTATIONS --- //

void errands_new_list_dialog_build() {
  // Box
  GtkWidget *box = gtk_list_box_new();
  g_object_set(box, "selection-mode", GTK_SELECTION_NONE, NULL);
  gtk_widget_add_css_class(box, "boxed-list");

  // Entry
  new_list_dialog_entry = adw_entry_row_new();
  g_object_set(new_list_dialog_entry, "title", "List Name", NULL);
  g_signal_connect(new_list_dialog_entry, "changed",
                   G_CALLBACK(on_entry_changed_cb), NULL);
  g_signal_connect(new_list_dialog_entry, "entry-activated",
                   G_CALLBACK(on_entry_activate_cb), NULL);
  gtk_list_box_append(GTK_LIST_BOX(box), new_list_dialog_entry);

  // Dialog
  new_list_dialog = g_object_ref_sink(adw_alert_dialog_new("Add List", ""));
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(new_list_dialog), "cancel",
                                "Cancel");
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(new_list_dialog), "create",
                                "Create");
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(new_list_dialog),
                                           "create", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(new_list_dialog),
                                        "cancel");
  adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(new_list_dialog),
                                      "cancel");
  adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(new_list_dialog), box);
  adw_alert_dialog_set_response_enabled(ADW_ALERT_DIALOG(new_list_dialog),
                                        "create", false);
  g_signal_connect(new_list_dialog, "response", G_CALLBACK(on_response_cb),
                   NULL);
}

void errands_new_list_dialog_show(GtkButton *btn) {
  adw_dialog_present(new_list_dialog, state.main_window);
  gtk_editable_set_text(GTK_EDITABLE(new_list_dialog_entry), "");
  gtk_widget_grab_focus(new_list_dialog_entry);
}

// --- SIGNAL HANDLERS --- //

static void on_entry_changed_cb(AdwEntryRow *entry) {
  adw_alert_dialog_set_response_enabled(
      ADW_ALERT_DIALOG(new_list_dialog), "create",
      strcmp("", gtk_editable_get_text(GTK_EDITABLE(entry))));
}

static void on_entry_activate_cb(AdwEntryRow *entry) {
  const char *text = gtk_editable_get_text(GTK_EDITABLE(new_list_dialog_entry));
  if (!strcmp(text, ""))
    return;
  on_response_cb(ADW_ALERT_DIALOG(new_list_dialog), "create", NULL);
  adw_dialog_close(new_list_dialog);
}

static void on_response_cb(AdwAlertDialog *dialog, gchar *response,
                           gpointer data) {
  if (!strcmp(response, "create")) {
    TaskListData *tld = errands_data_add_list(
        gtk_editable_get_text(GTK_EDITABLE(new_list_dialog_entry)));
    GtkWidget *row = errands_sidebar_add_task_list(tld);
    g_signal_emit_by_name(row, "activate", NULL);
  }
}
