#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_MONTH_CHOOSER (errands_month_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsMonthChooser, errands_month_chooser, ERRANDS, MONTH_CHOOSER,
                     GtkListBoxRow)

struct _ErrandsMonthChooser {
  GtkListBoxRow parent_instance;
  GtkWidget *month_box1;
  GtkWidget *month_box2;
};

ErrandsMonthChooser *errands_month_chooser_new();
const char *errands_month_chooser_get_months(ErrandsMonthChooser *mc);
void errands_month_chooser_set_months(ErrandsMonthChooser *mc, const char *rrule);
void errands_month_chooser_reset(ErrandsMonthChooser *mc);
