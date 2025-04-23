#include "task.h"

#include <glib/gi18n.h>

static void on_time_set_cb(ErrandsTimeChooser *tc);
static void on_time_preset_cb(GtkButton *btn, const char *h);

G_DEFINE_TYPE(ErrandsTimeChooser, errands_time_chooser, GTK_TYPE_BOX)

static void errands_time_chooser_class_init(ErrandsTimeChooserClass *class) {}

static void errands_time_chooser_init(ErrandsTimeChooser *self) {
  g_object_set(self, "spacing", 6, NULL);

  GtkWidget *morning_btn = gtk_button_new();
  GtkWidget *morning_btn_content = adw_button_content_new();
  g_object_set(morning_btn_content, "icon-name", "errands-morning-symbolic", "label", "9:00", NULL);
  g_object_set(morning_btn, "child", morning_btn_content, NULL);
  g_signal_connect(morning_btn, "clicked", G_CALLBACK(on_time_preset_cb), "9");

  GtkWidget *afternoon_btn = gtk_button_new();
  GtkWidget *afternoon_btn_content = adw_button_content_new();
  g_object_set(afternoon_btn_content, "icon-name", "errands-afternoon-symbolic", "label", "12:00", NULL);
  g_object_set(afternoon_btn, "child", afternoon_btn_content, NULL);
  g_signal_connect(afternoon_btn, "clicked", G_CALLBACK(on_time_preset_cb), "12");

  GtkWidget *sunset_btn = gtk_button_new();
  GtkWidget *sunset_btn_content = adw_button_content_new();
  g_object_set(sunset_btn_content, "icon-name", "errands-sunset-symbolic", "label", "17:00", NULL);
  g_object_set(sunset_btn, "child", sunset_btn_content, NULL);
  g_signal_connect(sunset_btn, "clicked", G_CALLBACK(on_time_preset_cb), "17");

  GtkWidget *night_btn = gtk_button_new();
  GtkWidget *night_btn_content = adw_button_content_new();
  g_object_set(night_btn_content, "icon-name", "errands-night-symbolic", "label", "20:00", NULL);
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
  g_signal_connect_swapped(self->hours, "value-changed", G_CALLBACK(on_time_set_cb), self);

  self->minutes = gtk_spin_button_new_with_range(0, 59, 1);
  g_object_set(self->minutes, "orientation", GTK_ORIENTATION_VERTICAL, NULL);
  g_signal_connect_swapped(self->minutes, "value-changed", G_CALLBACK(on_time_set_cb), self);

  GtkWidget *clock_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append(GTK_BOX(clock_hbox), self->hours);
  gtk_box_append(GTK_BOX(clock_hbox), gtk_label_new(":"));
  gtk_box_append(GTK_BOX(clock_hbox), self->minutes);
  gtk_box_append(GTK_BOX(clock_hbox), clock_preset_vbox1);
  gtk_box_append(GTK_BOX(clock_hbox), clock_preset_vbox2);

  // Reset button
  self->reset_btn = g_object_new(GTK_TYPE_BUTTON, "icon-name", "errands-reset-symbolic", "tooltip-text", _("Reset"),
                                 "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(self->reset_btn, "flat");
  g_signal_connect_swapped(self->reset_btn, "clicked", G_CALLBACK(errands_time_chooser_reset), self);
  gtk_box_append(GTK_BOX(self), self->reset_btn);

  // Label
  self->label = gtk_label_new(_("Not Set"));
  gtk_widget_add_css_class(self->label, "numeric");
  gtk_box_append(GTK_BOX(self), self->label);

  // Button
  GtkWidget *btn =
      g_object_new(GTK_TYPE_MENU_BUTTON, "popover", g_object_new(GTK_TYPE_POPOVER, "child", clock_hbox, NULL),
                   "icon-name", "errands-clock-symbolic", "valign", GTK_ALIGN_CENTER, NULL);
  gtk_widget_add_css_class(btn, "flat");
  gtk_box_append(GTK_BOX(self), btn);
}

ErrandsTimeChooser *errands_time_chooser_new() { return g_object_new(ERRANDS_TYPE_TIME_CHOOSER, NULL); }

const char *errands_time_chooser_get_time(ErrandsTimeChooser *tc) {
  static char time[7];
  sprintf(time, "%02d%02d00", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tc->hours)),
          gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tc->minutes)));
  return time;
}

void errands_time_chooser_set_time(ErrandsTimeChooser *tc, const char *hhmmss) {
  char h[3], m[3];
  sprintf(h, "%c%c", hhmmss[0], hhmmss[1]);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tc->hours), atoi(h));
  sprintf(m, "%c%c", hhmmss[2], hhmmss[3]);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tc->minutes), atoi(m));
}

void errands_time_chooser_reset(ErrandsTimeChooser *tc) {
  errands_time_chooser_set_time(tc, "000000");
  g_object_set(tc->label, "label", _("Not Set"), NULL);
  g_object_set(tc->reset_btn, "visible", false, NULL);
}

// --- SIGNAL HANDLERS --- //

static void on_time_set_cb(ErrandsTimeChooser *tc) {
  char m[3], h[3], time[6];
  sprintf(m, "%02d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tc->minutes)));
  gtk_editable_set_text(GTK_EDITABLE(tc->minutes), m);
  sprintf(h, "%02d", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tc->hours)));
  gtk_editable_set_text(GTK_EDITABLE(tc->hours), h);
  sprintf(time, "%s:%s", h, m);
  gtk_label_set_label(GTK_LABEL(tc->label), time);
  g_object_set(tc->reset_btn, "visible", true, NULL);
}

static void on_time_preset_cb(GtkButton *btn, const char *h) {
  ErrandsTimeChooser *tc = ERRANDS_TIME_CHOOSER(gtk_widget_get_ancestor(GTK_WIDGET(btn), ERRANDS_TYPE_TIME_CHOOSER));
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tc->minutes), 0);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(tc->hours), atoi(h));
}
