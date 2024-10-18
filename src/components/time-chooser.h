#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_TIME_CHOOSER (errands_time_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTimeChooser, errands_time_chooser, ERRANDS,
                     TIME_CHOOSER, GtkBox)

struct _ErrandsTimeChooser {
  GtkBox parent_instance;
  GtkWidget *hours;
  GtkWidget *minutes;
  GString *time; // Time string in HHMMSS format
  bool block_signals;
};

ErrandsTimeChooser *errands_time_chooser_new();
void errands_time_chooser_set_time(ErrandsTimeChooser *tc, const char *hhmmss);
char *errands_time_chooser_get_time(ErrandsTimeChooser *tc);
