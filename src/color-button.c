// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "color-button.h"
#include "config.h"
#include "state.h"
#include "utils.h"

#include <gdk/gdk.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

static void on_click_cb(ErrandsColorButton *self);

// ---------- WIDGET TEMPLATE ---------- //

struct _ErrandsColorButton {
  AdwBin parent_instance;

  // Properties
  GdkRGBA rgba;
};

G_DEFINE_TYPE(ErrandsColorButton, errands_color_button, GTK_TYPE_BUTTON)

enum {
  PROP_0,

  PROP_RGBA,

  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsColorButton *self = ERRANDS_COLOR_BUTTON(object);
  switch (prop_id) {
  case PROP_RGBA: g_value_set_pointer(value, &self->rgba); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsColorButton *self = ERRANDS_COLOR_BUTTON(object);
  switch (prop_id) {
  case PROP_RGBA: {
    self->rgba = *((GdkRGBA *)g_value_get_pointer(value));
    gtk_widget_set_color(GTK_WIDGET(self), gdk_rgba_to_str(&self->rgba));
  } break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void finalize(GObject *object) { G_OBJECT_CLASS(errands_color_button_parent_class)->finalize(object); }

static void errands_color_button_class_init(ErrandsColorButtonClass *class) {
  GObjectClass *object_class = G_OBJECT_CLASS(class);
  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  obj_properties[PROP_RGBA] = g_param_spec_pointer("rgba", "RGBA", "RGBA", G_PARAM_READWRITE);
  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void errands_color_button_init(ErrandsColorButton *self) {
  gtk_widget_set_name(GTK_WIDGET(self), "errands-color-button");
  gtk_widget_set_tooltip_text(GTK_WIDGET(self), _("Select Color"));
  gtk_widget_set_valign(GTK_WIDGET(self), GTK_ALIGN_CENTER);
  gtk_widget_add_css_string(GTK_WIDGET(self), "#errands-color-button {"
                                              "border-width: 1px;"
                                              "border-color: var(--border-color);"
                                              "border-style: solid;"
                                              "transform: scale(0.8);"
                                              "}");
  gtk_widget_set_cursor(GTK_WIDGET(self), gdk_cursor_new_from_name("pointer", NULL));
  gtk_widget_add_css_class(GTK_WIDGET(self), "circular");

  if (!state.color_dialog) state.color_dialog = gtk_color_dialog_new();
  self->rgba = (GdkRGBA){0};
  gtk_color_dialog_set_with_alpha(state.color_dialog, FALSE);

  g_signal_connect_swapped(self, "clicked", G_CALLBACK(on_click_cb), self);
}

ErrandsColorButton *errands_color_button_new() { return g_object_new(ERRANDS_TYPE_COLOR_BUTTON, NULL); }

// ---------- PUBLIC FUNCTIONS ---------- //

GdkRGBA errands_color_button_get_rgba(ErrandsColorButton *self) { return self->rgba; }
void errands_color_button_set_rgba(ErrandsColorButton *self, GdkRGBA *rgba) { g_object_set(self, "rgba", rgba, NULL); }

// ---------- CALLBACKS ---------- //

static void on_finish_cb(GObject *source_object, GAsyncResult *res, gpointer data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GdkRGBA) rgba = gtk_color_dialog_choose_rgba_finish(GTK_COLOR_DIALOG(source_object), res, &error);
  if (error) return;
  g_object_set(data, "rgba", rgba, NULL);
}

static void on_click_cb(ErrandsColorButton *self) {
  gtk_color_dialog_choose_rgba(state.color_dialog, GTK_WINDOW(state.main_window), &self->rgba, NULL, on_finish_cb, self);
}
