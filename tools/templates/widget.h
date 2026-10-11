// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_WIDGET (errands_widget_get_type())
G_DECLARE_FINAL_TYPE(ErrandsWidget, errands_widget, ERRANDS, WIDGET, AdwBin)

ErrandsWidget *errands_widget_new();
