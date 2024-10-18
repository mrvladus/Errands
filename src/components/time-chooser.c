#include "time-chooser.h"
#include "glib-object.h"
#include <stdbool.h>
// #include "../utils.h"

static void on_time_set_cb(ErrandsTimeChooser *tc);
static void on_time_preset_cb(GtkButton *btn, const char *h);

G_DEFINE_TYPE(ErrandsTimeChooser, errands_time_chooser, GTK_TYPE_BOX)

enum { CHANGED, LAST_SIGNAL };

static guint signals[LAST_SIGNAL] = {0};

static void errands_time_chooser_class_init(ErrandsTimeChooserClass *class) {
  signals[CHANGED] =
      g_signal_newv("changed", G_TYPE_FROM_CLASS(class),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                    NULL, NULL, NULL, NULL, G_TYPE_NONE, 0, NULL);
}

static void errands_time_chooser_init(ErrandsTimeChooser *self) {
  g_object_set(self, "spacing", 6, NULL);
  self->block_signals = false;
  self->time = g_string_new("------");

  GtkWidget *morning_btn = gtk_button_new();
  GtkWidget *morning_btn_content = adw_button_content_new();
  g_object_set(morning_btn_content, "icon-name", "errands-morning-symbolic",
               "label", "9:00", NULL);
  g_object_set(morning_btn, "child", morning_btn_content, NULL);
  g_signal_connect(morning_btn, "clicked", G_CALLBACK(on_time_preset_cb), "9");

  GtkWidget *afternoon_btn = gtk_button_new();
  GtkWidget *afternoon_btn_content = adw_button_content_new();
  g_object_set(afternoon_btn_content, "icon-name", "errands-afternoon-symbolic",
               "label", "12:00", NULL);
  g_object_set(afternoon_btn, "child", afternoon_btn_content, NULL);
  g_signal_connect(afternoon_btn, "clicked", G_CALLBACK(on_time_preset_cb),
                   "12");

  GtkWidget *sunset_btn = gtk_button_new();
  GtkWidget *sunset_btn_content = adw_button_content_new();
  g_object_set(sunset_btn_content, "icon-name", "errands-sunset-symbolic",
               "label", "17:00", NULL);
  g_object_set(sunset_btn, "child", sunset_btn_content, NULL);
  g_signal_connect(sunset_btn, "clicked", G_CALLBACK(on_time_preset_cb), "17");

  GtkWidget *night_btn = gtk_button_new();
  GtkWidget *night_btn_content = adw_button_content_new();
  g_object_set(night_btn_content, "icon-name", "errands-night-symbolic",
               "label", "20:00", NULL);
  g_object_set(night_btn, "child", night_btn_content, NULL);
  g_signal_connect(night_btn, "clicked", G_CALLBACK(on_time_preset_cb), "20");

  GtkWidget *clock_preset_vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  g_object_set(clock_preset_vbox1, "homogeneous", true, NULL);
  gtk_box_append(GTK_BOX(clock_preset_vbox1), morning_btn);
  gtk_box_append(GTK_BOX(clock_preset_vbox1), afternoon_btn);
  GtkWidget *clock_preset_vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  g_object_set(clock_preset_vbox2, "homogeneous", true, NULL);
  gtk_box_append(GTK_BOX(clock_preset_vbox2), sunset_btn);
  gtk_box_append(GTK_BOX(clock_preset_vbox2), night_btn);

  self->hours = gtk_spin_button_new_with_range(0, 23, 1);
  g_object_set(self->hours, "orientation", GTK_ORIENTATION_VERTICAL, NULL);
  g_signal_connect_swapped(self->hours, "value-changed",
                           G_CALLBACK(on_time_set_cb), self);

  self->minutes = gtk_spin_button_new_with_range(0, 59, 1);
  g_object_set(self->minutes, "orientation", GTK_ORIENTATION_VERTICAL, NULL);
  g_signal_connect_swapped(self->minutes, "value-changed",
                           G_CALLBACK(on_time_set_cb), self);

  GtkWidget *clock_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append(GTK_BOX(self), self->hours);
  gtk_box_append(GTK_BOX(self), gtk_label_new(":"));
  gtk_box_append(GTK_BOX(self), self->minutes);
  gtk_box_append(GTK_BOX(self), clock_preset_vbox1);
  gtk_box_append(GTK_BOX(self), clock_preset_vbox2);
}

ErrandsTimeChooser *errands_time_chooser_new() {
  return g_object_new(ERRANDS_TYPE_TIME_CHOOSER, NULL);
}

void errands_time_chooser_set_time(ErrandsTimeChooser *tc, const char *hhmmss) {
  tc->block_signals = true;
  char h[3];
  sprintf(h, "%c%c", hhmmss[0], hhmmss[1]);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tc->hours), atoi(h));
  tc->block_signals = false;
  char m[3];
  sprintf(h, "%c%c", hhmmss[2], hhmmss[3]);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tc->minutes), atoi(m));
}

static void on_time_set_cb(ErrandsTimeChooser *tc) {
  gchar *m = g_strdup_printf(
      "%02d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tc->minutes)));
  gtk_editable_set_text(GTK_EDITABLE(tc->minutes), m);
  gchar *h = g_strdup_printf(
      "%02d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tc->hours)));
  gchar *hms = g_strdup_printf("%s%s00", h, m);
  g_string_overwrite(tc->time, 0, hms);
  g_free(m);
  g_free(h);
  g_free(hms);
  if (!tc->block_signals)
    g_signal_emit_by_name(tc, "changed", NULL);
}

static void on_time_preset_cb(GtkButton *btn, const char *h) {
  ErrandsTimeChooser *tc = ERRANDS_TIME_CHOOSER(
      gtk_widget_get_ancestor(GTK_WIDGET(btn), ERRANDS_TYPE_TIME_CHOOSER));
  tc->block_signals = true;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tc->minutes), 0);
  tc->block_signals = false;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tc->hours), atoi(h));
}
