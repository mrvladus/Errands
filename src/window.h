// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include "date-chooser.h"
#include "sidebar.h"
#include "task-item.h"
#include "task-list.h"

#include <adwaita.h>
#include <gtksourceview/gtksource.h>
#include <libical/ical.h>

// --- RRULE ROW --- //

#define ERRANDS_TYPE_RRULE_ROW (errands_rrule_row_get_type())
G_DECLARE_FINAL_TYPE(ErrandsRruleRow, errands_rrule_row, ERRANDS, TASK_RRULE_ROW, AdwExpanderRow)

ErrandsRruleRow *errands_rrule_row_new();
struct icalrecurrencetype *errands_rrule_row_get_rrule(ErrandsRruleRow *self);
void errands_rrule_row_set_rrule(ErrandsRruleRow *self, const struct icalrecurrencetype *rrule);
void errands_rrule_row_reset(ErrandsRruleRow *self);

// --- WINDOW --- //

#define ERRANDS_TYPE_WINDOW (errands_window_get_type())
G_DECLARE_FINAL_TYPE(ErrandsWindow, errands_window, ERRANDS, WINDOW, AdwApplicationWindow)

struct _ErrandsWindow {
  AdwApplicationWindow parent_instance;
  ErrandsTaskList *task_list;
  AdwToastOverlay *toast_overlay;
  ErrandsSidebar *sidebar;
  AdwNavigationSplitView *split_view;
};

ErrandsWindow *errands_window_new(GtkApplication *app);
void errands_window_add_toast(const char *msg, int timeout);
