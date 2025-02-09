#pragma once

#include <gtk/gtk.h>

#define ERRANDS_TYPE_DATE_CHOOSER (errands_date_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsDateChooser, errands_date_chooser, ERRANDS, DATE_CHOOSER, GtkBox)

struct _ErrandsDateChooser {
  GtkBox parent_instance;
  GtkWidget *calendar;
  GtkWidget *label;
  GtkWidget *reset_btn;
};

ErrandsDateChooser *errands_date_chooser_new();
// Get string in format YYYYMMDD
const char *errands_date_chooser_get_date(ErrandsDateChooser *dc);
void errands_date_chooser_set_date(ErrandsDateChooser *dc, const char *yyyymmss);
void errands_date_chooser_reset(ErrandsDateChooser *dc);
