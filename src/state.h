// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include "delete-list-dialog.h"
#include "rename-list-dialog.h"
#include "settings-dialog.h"
#include "settings.h"
#include "window.h"

#include <glib.h>
#include <gtk/gtk.h>

typedef struct {
  AdwApplication *app;
  ErrandsWindow *main_window;
  ErrandsSettings *settings;
  ErrandsSettingsDialog *settings_dialog;
  ErrandsRenameListDialog *rename_list_dialog;
  ErrandsDeleteListDialog *delete_list_dialog;
  GtkColorDialog *color_dialog;

  // CLI flags
  gboolean sync_and_quit;
} State;

// Global state object
extern State state;

static inline void errands_state_cleanup() {
  g_clear_object(&state.color_dialog);
  g_clear_object(&state.settings);
  g_clear_object(&state.app);
}
