// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include "delete-list-dialog.h"
#include "rename-list-dialog.h"
#include "settings-dialog.h"
#include "settings.h"
#include "window.h"

typedef struct {
  AdwApplication *app;
  ErrandsWindow *main_window;
  ErrandsSettings *settings;
  ErrandsSettingsDialog *settings_dialog;
  ErrandsRenameListDialog *rename_list_dialog;
  ErrandsDeleteListDialog *delete_list_dialog;
} State;

// Global state object
extern State state;
