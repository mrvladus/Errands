#include "settings-dialog.h"
#include "adwaita.h"
#include "glib-object.h"
#include "settings.h"
#include "state.h"

static ErrandsSettingsDialog *settings_dialog = NULL;

static void on_theme_toggled_cb(ErrandsSettingsDialog *self);
static void on_notifications_toggled_cb(ErrandsSettingsDialog *self);
static void on_background_toggled_cb(ErrandsSettingsDialog *self);
static void on_startup_toggled_cb(ErrandsSettingsDialog *self);
static void on_sync_toggled_cb(ErrandsSettingsDialog *self);

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
  gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class),
                                              "/io/github/mrvladus/Errands/ui/settings-dialog.ui");
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, theme);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, light);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, dark);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, notifications);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, background);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, startup);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, sync_enabled);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, sync_url);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, sync_username);
  gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), ErrandsSettingsDialog, sync_password);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_theme_toggled_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_notifications_toggled_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_background_toggled_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_startup_toggled_cb);
  gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), on_sync_toggled_cb);
}

static void errands_settings_dialog_init(ErrandsSettingsDialog *self) { gtk_widget_init_template(GTK_WIDGET(self)); }

ErrandsSettingsDialog *errands_settings_dialog_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_SETTINGS_DIALOG, NULL));
}

// ---------- PUBLIC FUNCTIONS ---------- //

void errands_settings_dialog_show() {
  if (!settings_dialog) settings_dialog = errands_settings_dialog_new();
  adw_switch_row_set_active(ADW_SWITCH_ROW(settings_dialog->notifications),
                            errands_settings_get(SETTING_NOTIFICATIONS).b);
  adw_switch_row_set_active(ADW_SWITCH_ROW(settings_dialog->background), errands_settings_get(SETTING_BACKGROUND).b);
  adw_switch_row_set_active(ADW_SWITCH_ROW(settings_dialog->startup), errands_settings_get(SETTING_STARTUP).b);
  adw_switch_row_set_active(ADW_SWITCH_ROW(settings_dialog->sync_enabled), errands_settings_get(SETTING_SYNC).b);
  adw_toggle_group_set_active(ADW_TOGGLE_GROUP(settings_dialog->theme), errands_settings_get(SETTING_THEME).i);

  adw_dialog_present(ADW_DIALOG(settings_dialog), GTK_WIDGET(state.main_window));
}

// ---------- CALLBACKS ---------- //

static void on_theme_toggled_cb(ErrandsSettingsDialog *self) {
  int theme = adw_toggle_group_get_active(ADW_TOGGLE_GROUP(self->theme));
  errands_settings_set(SETTING_THEME, &theme);
  AdwStyleManager *style_manager = adw_style_manager_get_default();
  switch (theme) {
  case SETTING_THEME_SYSTEM: adw_style_manager_set_color_scheme(style_manager, ADW_COLOR_SCHEME_DEFAULT); break;
  case SETTING_THEME_LIGHT: adw_style_manager_set_color_scheme(style_manager, ADW_COLOR_SCHEME_FORCE_LIGHT); break;
  case SETTING_THEME_DARK: adw_style_manager_set_color_scheme(style_manager, ADW_COLOR_SCHEME_FORCE_DARK); break;
  }
}

static void on_notifications_toggled_cb(ErrandsSettingsDialog *self) {
  bool enabled = adw_switch_row_get_active(ADW_SWITCH_ROW(self->notifications));
  errands_settings_set(SETTING_NOTIFICATIONS, &enabled);
}

static void on_background_toggled_cb(ErrandsSettingsDialog *self) {
  bool enabled = adw_switch_row_get_active(ADW_SWITCH_ROW(self->background));
  errands_settings_set(SETTING_BACKGROUND, &enabled);
}

static void on_startup_toggled_cb(ErrandsSettingsDialog *self) {
  bool enabled = adw_switch_row_get_active(ADW_SWITCH_ROW(self->startup));
  errands_settings_set(SETTING_STARTUP, &enabled);
}

static void on_sync_toggled_cb(ErrandsSettingsDialog *self) {
  bool enabled = adw_switch_row_get_active(ADW_SWITCH_ROW(self->sync_enabled));
  errands_settings_set(SETTING_SYNC, &enabled);
}
