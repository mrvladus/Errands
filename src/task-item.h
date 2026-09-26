#pragma once

#include "task-list-item.h"

#include <libical/ical.h>

#define ERRANDS_TYPE_TASK_ITEM (errands_task_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskItem, errands_task_item, ERRANDS, TASK_ITEM, GObject)

ErrandsTaskItem *errands_task_item_new(icalcomponent *ical, ErrandsTaskListItem *list, ErrandsTaskItem *parent);
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

// --- GETTERS --- //

ErrandsTaskListItem *errands_task_item_get_list(ErrandsTaskItem *self);
const char *errands_task_item_get_uid(ErrandsTaskItem *self);
const char *errands_task_item_get_title(ErrandsTaskItem *self);
icalcomponent *errands_task_item_get_ical(ErrandsTaskItem *self);
gboolean errands_task_item_get_completed(ErrandsTaskItem *self);
gboolean errands_task_item_get_cancelled(ErrandsTaskItem *self);
gint errands_task_item_get_priority(ErrandsTaskItem *self);
const char *errands_task_item_get_priority_as_string(ErrandsTaskItem *self);
// Get priority as translated string
const char *errands_task_item_get_priority_as_tstring(ErrandsTaskItem *self);
const char *errands_task_item_get_color(ErrandsTaskItem *self);
const char *errands_task_item_get_notes(ErrandsTaskItem *self);
const char *errands_task_item_get_search_blob(ErrandsTaskItem *self);
icaltimetype errands_task_item_get_dtstart(ErrandsTaskItem *self);
icaltimetype errands_task_item_get_dtend(ErrandsTaskItem *self);
GStrv errands_task_item_get_tags(ErrandsTaskItem *self);
GStrv errands_task_item_get_attachments(ErrandsTaskItem *self);
const struct icalrecurrencetype *errands_task_item_get_rrule(ErrandsTaskItem *self);
// Returns the rrule as a human-readable string, or NULL if no rrule is set.
// The caller owns the returned string and must free it with g_free().
gchar *errands_task_item_get_rrule_as_string(ErrandsTaskItem *self);
ErrandsTaskItem *errands_task_item_get_parent(ErrandsTaskItem *self);
GListModel *errands_task_item_get_children_model(ErrandsTaskItem *self);
gboolean errands_task_item_get_search_matched(ErrandsTaskItem *self);

int errands_task_item_get_indent_level(ErrandsTaskItem *self);

// --- SETTERS --- //

void errands_task_item_set_parent(ErrandsTaskItem *self, ErrandsTaskItem *parent);
void errands_task_item_set_title(ErrandsTaskItem *self, const char *title);
void errands_task_item_set_completed(ErrandsTaskItem *self, gboolean completed);
void errands_task_item_set_cancelled(ErrandsTaskItem *self, gboolean cancelled);
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
void errands_task_item_set_rrule(ErrandsTaskItem *self, const struct icalrecurrencetype *rrule);
void errands_task_item_set_search_matched(ErrandsTaskItem *self, gboolean matched);

// void errands_task_data_get_flat_list(TaskData *parent, GPtrArray *array) {
//   for_range(i, 0, parent->children->len) {
//     TaskData *sub_task = g_ptr_array_index(parent->children, i);
//     g_ptr_array_add(array, sub_task);
//     errands_task_data_get_flat_list(sub_task, array);
//   }
// }

// bool errands_task_data_move_to_list(TaskData *data, ListData *list, TaskData *parent) {
//   if (!data || !list || (data->parent && data->parent == parent)) return false;

//   GPtrArray *arr_to_remove_from = data->parent ? data->parent->children : data->list->children;
//   icalcomponent *clone = icalcomponent_clone(data->ical);
//   icalcomponent_remove_component(data->list->ical, data->ical);
//   icalcomponent_add_component(list->ical, clone);
//   data->ical = clone;
//   data->parent = parent;
//   errands_data_set_parent(clone, parent ? errands_data_get_uid(parent->ical) : NULL);

//   g_autoptr(GPtrArray) children = g_ptr_array_sized_new(data->children->len);
//   errands_task_data_get_flat_list(data, children);
//   for_range(i, 0, children->len) {
//     TaskData *child = g_ptr_array_index(children, i);
//     icalcomponent *child_clone = icalcomponent_clone(child->ical);
//     icalcomponent_remove_component(data->list->ical, child->ical);
//     icalcomponent_add_component(list->ical, child_clone);
//     child->ical = child_clone;
//     child->list = list;
//   }
//   data->list = list;

//   GPtrArray *arr_to_move_to = parent ? parent->children : list->children;
//   guint idx;
//   g_ptr_array_find(arr_to_remove_from, data, &idx);
//   g_ptr_array_add(arr_to_move_to, g_ptr_array_steal_index_fast(arr_to_remove_from, idx));

//   return true;
// }

// TaskData *errands_task_data_find_by_uid(ListData *list, const char *uid) {
//   g_autoptr(GPtrArray) tasks = g_ptr_array_sized_new(list->children->len);
//   errands_list_data_get_flat_list(list, tasks);
//   for_range(i, 0, tasks->len) {
//     TaskData *task = g_ptr_array_index(tasks, i);
//     if (STR_EQUAL(errands_data_get_uid(task->ical), uid)) return task;
//   }
//   return NULL;
// }
