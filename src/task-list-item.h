#pragma once

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <libical/ical.h>

#define ERRANDS_TYPE_TASK_LIST_ITEM (errands_task_list_item_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListItem, errands_task_list_item, ERRANDS, TASK_LIST_ITEM, GObject)

struct _ErrandsTaskListItem {
  GObject parent_instance;

  GListStore *tasks;

  // GObject Properties
  icalcomponent *ical;
  gchar *uid;
  gchar *title;
  GdkRGBA color;
  gint count; // Number of uncompleted tasks
};

ErrandsTaskListItem *errands_task_list_item_new(icalcomponent *ical);
ErrandsTaskListItem *errands_task_list_item_create(const char *uid, const char *name, const char *color, bool deleted,
                                                   bool synced);
ErrandsTaskListItem *errands_task_list_item_load_from_ics(const char *path);
void errands_task_list_item_update_count(ErrandsTaskListItem *self);
void errands_task_list_item_save(ErrandsTaskListItem *self);
void errands_task_list_item_delete(ErrandsTaskListItem *self);
int errands_task_list_item_delete_completed(ErrandsTaskListItem *self);
int errands_task_list_item_delete_cancelled(ErrandsTaskListItem *self);
void errands_task_list_item_remove_deleted_tasks(ErrandsTaskListItem *self);
// Creates a new task item with the given text, saves the list and adds it to the list children model.
struct ErrandsTaskItem *errands_task_list_item_create_task(ErrandsTaskListItem *self, struct ErrandsTaskItem *parent,
                                                           const char *text);
