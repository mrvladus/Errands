#pragma once

#include "data.h"
#include "glib.h"

#include <gdk/gdk.h>

#define ERRANDS_TYPE_TASK_ITEM (errands_task_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskItem, errands_task_item, ERRANDS, TASK_ITEM, GObject)

ErrandsTaskItem *errands_task_item_new(TaskData *data, ErrandsTaskItem *parent);

ErrandsTaskItem *errands_task_item_add_child(ErrandsTaskItem *self, TaskData *data);
// Update "uncompleted-count" and "has-no-children" properties
void errands_task_item_update(ErrandsTaskItem *self);
bool errands_task_item_is_due(ErrandsTaskItem *self);
void errands_task_item_add_tag(ErrandsTaskItem *self, const char *tag);
void errands_task_item_remove_tag(ErrandsTaskItem *self, const char *tag);

const char *errands_task_item_get_title(ErrandsTaskItem *self);
gint errands_task_item_get_priority(ErrandsTaskItem *self);
const char *errands_task_item_get_priority_as_string(ErrandsTaskItem *self);
// Get priority as translated string
const char *errands_task_item_get_priority_as_tstring(ErrandsTaskItem *self);
const char *errands_task_item_get_color(ErrandsTaskItem *self);
const char *errands_task_item_get_notes(ErrandsTaskItem *self);
icaltimetype errands_task_item_get_dtstart(ErrandsTaskItem *self);
icaltimetype errands_task_item_get_dtend(ErrandsTaskItem *self);
GStrv errands_task_item_get_tags(ErrandsTaskItem *self);
GStrv errands_task_item_get_attachments(ErrandsTaskItem *self);

ErrandsTaskItem *errands_task_item_get_parent(ErrandsTaskItem *self);
GListModel *errands_task_item_get_children_model(ErrandsTaskItem *self);
TaskData *errands_task_item_get_data(ErrandsTaskItem *self);

void errands_task_item_set_title(ErrandsTaskItem *self, const char *title);
void errands_task_item_set_priority(ErrandsTaskItem *self, gint priority);
void errands_task_item_set_priority_from_string(ErrandsTaskItem *self, const char *priority);
void errands_task_item_set_notes(ErrandsTaskItem *self, const char *notes);
void errands_task_item_set_color(ErrandsTaskItem *self, const char *color);
void errands_task_item_set_dtstart(ErrandsTaskItem *self, icaltimetype dtstart);
void errands_task_item_set_dtend(ErrandsTaskItem *self, icaltimetype dtend);
// Set the tags for the task item. Item takes ownership of the tags array.
void errands_task_item_set_tags(ErrandsTaskItem *self, GStrv tags);
// Set the attachments for the task item. Item takes ownership of the attachments array.
void errands_task_item_set_attachments(ErrandsTaskItem *self, GStrv attachments);
void errands_task_item_set_parent(ErrandsTaskItem *self, ErrandsTaskItem *parent);
