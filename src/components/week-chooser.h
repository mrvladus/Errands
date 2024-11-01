#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_WEEK_CHOOSER (errands_week_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsWeekChooser, errands_week_chooser, ERRANDS, WEEK_CHOOSER, GtkListBoxRow)

struct _ErrandsWeekChooser {
  GtkListBoxRow parent_instance;
  GtkWidget *week_box;
};

ErrandsWeekChooser *errands_week_chooser_new();
const char *errands_week_chooser_get_days(ErrandsWeekChooser *wc);
void errands_week_chooser_set_days(ErrandsWeekChooser *wc, const char *rrule);
void errands_week_chooser_reset(ErrandsWeekChooser *wc);
