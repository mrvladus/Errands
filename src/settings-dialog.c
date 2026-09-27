#include "settings-dialog.h"
#include "config.h"
#include "glib-object.h"
#include "glib.h"
#include "settings.h"
#include "state.h"

#include <libportal-gtk4/portal-gtk4.h>

static void on_startup_toggled_cb(ErrandsSettingsDialog *self);

static ErrandsSettingsDialog *settings_dialog = NULL;

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsSettingsDialog {
  AdwPreferencesDialog parent_instance;
  GtkWidget *theme;
  GtkWidget *light;
  GtkWidget *dark;
  GtkWidget *notifications;
  GtkWidget *background;
  GtkWidget *startup;
  GtkWidget *sync_enabled;
  GtkWidget *sync_interval;
  GtkWidget *sync_url;
  GtkWidget *sync_username;
  GtkWidget *sync_password;
};

G_DEFINE_TYPE(ErrandsSettingsDialog, errands_settings_dialog, ADW_TYPE_PREFERENCES_DIALOG)

static void errands_settings_dialog_dispose(GObject *gobject) {
  gtk_widget_dispose_template(GTK_WIDGET(gobject), ERRANDS_TYPE_SETTINGS_DIALOG);
  G_OBJECT_CLASS(errands_settings_dialog_parent_class)->dispose(gobject);
}

static void errands_settings_dialog_class_init(ErrandsSettingsDialogClass *class) {
  G_OBJECT_CLASS(class)->dispose = errands_settings_dialog_dispose;
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), RESOURCE_PATH "/ui/settings-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, theme);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, light);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, dark);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, notifications);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, background);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, startup);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, sync_enabled);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, sync_interval);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, sync_url);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, sync_username);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, sync_password);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_startup_toggled_cb);
}

static void errands_settings_dialog_init(ErrandsSettingsDialog *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

static gboolean theme_transform_func(GBinding *binding, const GValue *from_value, GValue *to_value, gpointer user_data) {
  gint value = g_value_get_int(from_value);
  g_value_set_uint(to_value, value == 2 ? 4 : value);
  return TRUE;
}

ErrandsSettingsDialog *errands_settings_dialog_new() {
  ErrandsSettingsDialog *dialog = g_object_ref_sink(g_object_new(ERRANDS_TYPE_SETTINGS_DIALOG, NULL));

  g_object_bind_property(state.settings, "background", dialog->background, "active",
                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "startup", dialog->startup, "active", G_BINDING_SYNC_CREATE);
  g_object_bind_property(state.settings, "sync-enabled", dialog->sync_enabled, "active",
                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "sync-interval", dialog->sync_interval, "value",
                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property_full(state.settings, "theme", dialog->theme, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                              (GBindingTransformFunc)theme_transform_func, NULL, NULL, NULL);
  g_object_bind_property(state.settings, "sync-url", dialog->sync_url, "text", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "sync-username", dialog->sync_username, "text",
                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  g_object_bind_property(state.settings, "sync-password", dialog->sync_password, "text",
                         G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  return dialog;
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_settings_dialog_show() {
  if (!settings_dialog) settings_dialog = errands_settings_dialog_new();
  adw_dialog_present(ADW_DIALOG(settings_dialog), GTK_WIDGET(state.main_window));
}

// ---------- CALLBACKS ---------- //

static void on_startup_toggled_cb(ErrandsSettingsDialog *self) {
  bool enabled = adw_switch_row_get_active(ADW_SWITCH_ROW(self->startup));
  errands_settings_set_startup(state.settings, enabled);
  g_autoptr(XdpPortal) portal = xdp_portal_new();
  g_autoptr(XdpParent) parent = xdp_parent_new_gtk(GTK_WINDOW(state.main_window));
  if (enabled) {
    g_autoptr(GPtrArray) cmdline = g_ptr_array_sized_new(2);
    g_ptr_array_add(cmdline, "errands");
    g_ptr_array_add(cmdline, "--gapplication-service");
    xdp_portal_request_background(portal, parent, "Errands needs to run in the background for sending notifications", cmdline,
                                  XDP_BACKGROUND_FLAG_AUTOSTART, NULL, NULL, NULL);
  } else xdp_portal_request_background(portal, parent, NULL, NULL, XDP_BACKGROUND_FLAG_NONE, NULL, NULL, NULL);
}

// static void on_notifications_toggled_cb(ErrandsSettingsDialog *self) {
//   bool enabled = adw_switch_row_get_active(ADW_SWITCH_ROW(self->notifications));
//   enabled ? errands_notifications_start() : errands_notifications_stop();
//   errands_settings_set(SETTING_NOTIFICATIONS, &enabled);
// }
