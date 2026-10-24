// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include <adwaita.h>
#include <libical/ical.h>

#define ERRANDS_TYPE_DATE_CHOOSER (errands_date_chooser_get_type())
G_DECLARE_FINAL_TYPE(ErrandsDateChooser, errands_date_chooser, ERRANDS, DATE_CHOOSER, AdwActionRow)

ErrandsDateChooser *errands_date_chooser_new();
void errands_date_chooser_reset(ErrandsDateChooser *self);
icaltimetype errands_date_chooser_get_dt(ErrandsDateChooser *self);
void errands_date_chooser_set_dt(ErrandsDateChooser *self, icaltimetype dt);
