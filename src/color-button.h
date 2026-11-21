// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_COLOR_BUTTON (errands_color_button_get_type())
G_DECLARE_FINAL_TYPE(ErrandsColorButton, errands_color_button, ERRANDS, COLOR_BUTTON, GtkButton)

ErrandsColorButton *errands_color_button_new();
GdkRGBA errands_color_button_get_rgba(ErrandsColorButton *self);
void errands_color_button_set_rgba(ErrandsColorButton *self, GdkRGBA *rgba);
