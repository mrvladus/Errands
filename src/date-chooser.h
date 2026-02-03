#pragma once

#include <adwaita.h>
#include <libical/ical.h>

#define ERRANDS_TYPE_DATE_CHOOSER (errands_date_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsDateChooser, errands_date_chooser, ERRANDS, DATE_CHOOSER, AdwActionRow)

ErrandsDateChooser *errands_date_chooser_new();
// Reset the date chooser to its initial state.
void errands_date_chooser_reset(ErrandsDateChooser *self);
// Get selected date and time as an icaltimetype. If only date is selected, is_date will be set to true.
icaltimetype errands_date_chooser_get_dt(ErrandsDateChooser *self);
// Set selected date and time from an icaltimetype.
void errands_date_chooser_set_dt(ErrandsDateChooser *self, const icaltimetype dt);
