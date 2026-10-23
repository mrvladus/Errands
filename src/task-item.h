// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include "task-list-item.h"

#define ERRANDS_TYPE_TASK_ITEM (errands_task_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskItem, errands_task_item, ERRANDS, TASK_ITEM, GObject)

// Takes reference of `ical`.
ErrandsTaskItem *errands_task_item_new(ICalComponent *ical, ErrandsTaskListItem *list, ErrandsTaskItem *parent);
ErrandsTaskListItem *errands_task_item_create_task(ErrandsTaskItem *parent, const char *text);

// Update "uncompleted-count" and "has-no-children" properties
void errands_task_item_update(ErrandsTaskItem *self);
bool errands_task_item_is_due(ErrandsTaskItem *self);
void errands_task_item_add_tag(ErrandsTaskItem *self, const char *tag);
void errands_task_item_remove_tag(ErrandsTaskItem *self, const char *tag);
void errands_task_item_delete(ErrandsTaskItem *self);
int errands_task_item_delete_completed(ErrandsTaskItem *self);
int errands_task_item_delete_cancelled(ErrandsTaskItem *self);
int errands_task_item_remove_deleted_tasks(ErrandsTaskItem *self);
bool errands_task_item_export(ErrandsTaskItem *self, const char *path);
bool errands_task_item_move(ErrandsTaskItem *self, ErrandsTaskItem *parent, ErrandsTaskListItem *list);

// --- GETTERS --- //

ErrandsTaskListItem *errands_task_item_get_list(ErrandsTaskItem *self);
const char *errands_task_item_get_uid(ErrandsTaskItem *self);
const char *errands_task_item_get_title(ErrandsTaskItem *self);
ICalComponent *errands_task_item_get_ical(ErrandsTaskItem *self);
gboolean errands_task_item_get_completed(ErrandsTaskItem *self);
gboolean errands_task_item_get_cancelled(ErrandsTaskItem *self);
gint errands_task_item_get_priority(ErrandsTaskItem *self);
const char *errands_task_item_get_priority_as_string(ErrandsTaskItem *self);
// Get priority as translated string
const char *errands_task_item_get_priority_as_tstring(ErrandsTaskItem *self);
const char *errands_task_item_get_color(ErrandsTaskItem *self);
const char *errands_task_item_get_notes(ErrandsTaskItem *self);
const char *errands_task_item_get_search_blob(ErrandsTaskItem *self);
ICalTime *errands_task_item_get_dtstart(ErrandsTaskItem *self);
ICalTime *errands_task_item_get_dtend(ErrandsTaskItem *self);
const GStrv errands_task_item_get_tags(ErrandsTaskItem *self);
const GStrv errands_task_item_get_attachments(ErrandsTaskItem *self);
ICalRecurrence *errands_task_item_get_rrule(ErrandsTaskItem *self);
// Returns the rrule as a human-readable string, or NULL if no rrule is set.
// The caller owns the returned string and must free it with g_free().
gchar *errands_task_item_get_rrule_as_string(ErrandsTaskItem *self);
ErrandsTaskItem *errands_task_item_get_parent(ErrandsTaskItem *self);
GListModel *errands_task_item_get_children_model(ErrandsTaskItem *self);
gboolean errands_task_item_get_search_matched(ErrandsTaskItem *self);
int errands_task_item_get_indent_level(ErrandsTaskItem *self);
GListStore *errands_task_item_get_parent_model(ErrandsTaskItem *self);

// --- SETTERS --- //

void errands_task_item_set_list(ErrandsTaskItem *self, ErrandsTaskListItem *list);
void errands_task_item_set_ical(ErrandsTaskItem *self, ICalComponent *ical);
void errands_task_item_set_parent(ErrandsTaskItem *self, ErrandsTaskItem *parent);
void errands_task_item_set_title(ErrandsTaskItem *self, const char *title);
void errands_task_item_set_completed(ErrandsTaskItem *self, gboolean completed);
void errands_task_item_set_cancelled(ErrandsTaskItem *self, gboolean cancelled);
void errands_task_item_set_priority(ErrandsTaskItem *self, gint priority);
void errands_task_item_set_priority_from_string(ErrandsTaskItem *self, const char *priority);
void errands_task_item_set_notes(ErrandsTaskItem *self, const char *notes);
void errands_task_item_set_color(ErrandsTaskItem *self, const char *color);
void errands_task_item_set_dtstart(ErrandsTaskItem *self, ICalTime *dtstart);
void errands_task_item_set_dtend(ErrandsTaskItem *self, ICalTime *dtend);
// Set the tags for the task item. Item takes ownership of the tags array.
void errands_task_item_set_tags(ErrandsTaskItem *self, const GStrv tags);
// Set the attachments for the task item. Item takes ownership of the attachments array.
void errands_task_item_set_attachments(ErrandsTaskItem *self, const GStrv attachments);
void errands_task_item_set_rrule(ErrandsTaskItem *self, const ICalRecurrence *rrule);
void errands_task_item_set_search_matched(ErrandsTaskItem *self, gboolean matched);
