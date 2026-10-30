// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "task-item.h"
#include "data.h"
#include "settings.h"
#include "state.h"
#include "sync.h"
#include "task-list-item.h"
#include "task-list.h"
#include "task.h"
#include "utils.h"
#include "window.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <libical/ical.h>

static void update_search_blob(ErrandsTaskItem *self);
static void on_global_tags_changed_cb(ErrandsTaskItem *self);

struct _ErrandsTaskItem {
  GObject parent_instance;

  bool lock;

  // Properties
  gchar *uid;
  gchar *title;
  gchar *color;
  gchar *notes;
  gboolean completed;
  gboolean cancelled;
  gint priority;
  icaltimetype dtstart;
  icaltimetype dtend;
  GStrv tags;
  GStrv attachments;
  struct icalrecurrencetype *rrule;
  gchar *search_blob;
  gboolean search_matched;

  gchar *uncompleted_count; // The number of uncompleted subtasks
  gboolean has_no_children; // If the task can be expanded (has any children)

  icalcomponent *ical;
  ErrandsTaskItem *parent;
  ErrandsTaskListItem *list;
  GListStore *children_model;

  ErrandsTask *task_widget;
};

G_DEFINE_TYPE(ErrandsTaskItem, errands_task_item, G_TYPE_OBJECT)

enum {
  PROP_0,

  PROP_UID,
  PROP_TITLE,
  PROP_COLOR,
  PROP_NOTES,
  PROP_COMPLETED,
  PROP_CANCELLED,
  PROP_PRIORITY,
  PROP_DTSTART,
  PROP_DTEND,
  PROP_TAGS,
  PROP_ATTACHMENTS,
  PROP_RRULE,
  PROP_SEARCH_BLOB,
  PROP_SEARCH_MATCHED,

  PROP_UNCOMPLETED_COUNT,
  PROP_HAS_NO_CHILDREN,

  PROP_ICAL,
  PROP_CHILDREN_MODEL,
  PROP_TASK_WIDGET,
  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};
static int update_task_list_count = 0;

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);
  switch (prop_id) {
  case PROP_UID: g_value_set_string(value, self->uid); break;
  case PROP_TITLE: g_value_set_string(value, self->title); break;
  case PROP_COMPLETED: g_value_set_boolean(value, self->completed); break;
  case PROP_CANCELLED: g_value_set_boolean(value, self->cancelled); break;
  case PROP_COLOR: g_value_set_string(value, self->color); break;
  case PROP_PRIORITY: g_value_set_int(value, self->priority); break;
  case PROP_NOTES: g_value_set_string(value, self->notes); break;
  case PROP_DTSTART: g_value_set_pointer(value, &self->dtstart); break;
  case PROP_DTEND: g_value_set_pointer(value, &self->dtend); break;
  case PROP_TAGS: g_value_set_pointer(value, self->tags); break;
  case PROP_ATTACHMENTS: g_value_set_pointer(value, self->attachments); break;
  case PROP_RRULE: g_value_set_pointer(value, self->rrule); break;
  case PROP_SEARCH_BLOB: g_value_set_string(value, self->search_blob); break;
  case PROP_SEARCH_MATCHED: g_value_set_boolean(value, self->search_matched); break;
  case PROP_UNCOMPLETED_COUNT: g_value_set_string(value, self->uncompleted_count); break;
  case PROP_HAS_NO_CHILDREN: g_value_set_boolean(value, self->has_no_children); break;
  case PROP_ICAL: g_value_set_pointer(value, self->ical); break;
  case PROP_CHILDREN_MODEL: g_value_set_object(value, self->children_model); break;
  case PROP_TASK_WIDGET: g_value_set_pointer(value, self->task_widget); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);
  switch (prop_id) {
  case PROP_TITLE: {
    const char *new_title = g_value_get_string(value);
    if (g_strcmp0(self->title, new_title) == 0) break;
    g_clear_pointer(&self->title, g_free);
    self->title = g_strdup(new_title);
    errands_data_set_text(self->ical, self->title);
    update_search_blob(self);
    if (!self->lock) errands_task_list_item_save(self->list);
  } break;

  case PROP_COMPLETED: {
    gboolean new_completed = g_value_get_boolean(value);
    if (self->completed == new_completed) break;
    self->completed = new_completed;
    update_task_list_count++;
    errands_data_set_completed(self->ical, self->completed ? icaltime_now() : icaltime_null_time());
    // Complete all sub-tasks if the task is completed
    if (self->completed) {
      GListStore *sub_tasks = self->children_model;
      for (guint i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(sub_tasks)); i++) {
        g_autoptr(ErrandsTaskItem) sub_task = g_list_model_get_item(G_LIST_MODEL(sub_tasks), i);
        g_object_set(sub_task, "completed", true, NULL);
      }
      errands_task_item_update(self);
      if (self->parent) errands_task_item_update(self->parent);
    }
    // Uncomplete all parents
    else {
      if (self->parent && self->parent->completed) {
        g_object_set(self->parent, "completed", false, NULL);
        errands_task_item_update(self->parent);
      }
    }
    if (update_task_list_count > 0) update_task_list_count--;
    if (update_task_list_count > 0) break;
    if (!self->lock) errands_task_list_item_save(self->list);
    errands_sidebar_update_filter_rows();
    errands_sidebar_task_list_update_counter(self->list->uid);
    errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_MORE_STRICT);
    errands_task_list_filter(state.main_window->task_list, GTK_FILTER_CHANGE_MORE_STRICT);
  } break;

  case PROP_CANCELLED: {
    gboolean new_cancelled = g_value_get_boolean(value);
    if (self->cancelled == new_cancelled) break;
    self->cancelled = new_cancelled;
    update_task_list_count++;
    errands_data_set_cancelled(self->ical, self->cancelled);
    if (self->cancelled) {
      GListStore *sub_tasks = self->children_model;
      for (guint i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(sub_tasks)); i++) {
        g_autoptr(ErrandsTaskItem) sub_task = g_list_model_get_item(G_LIST_MODEL(sub_tasks), i);
        g_object_set(sub_task, "cancelled", true, NULL);
      }
      errands_task_item_update(self);
      if (self->parent) errands_task_item_update(self->parent);
    } else {
      if (self->parent && self->parent->cancelled) {
        g_object_set(self->parent, "cancelled", false, NULL);
        errands_task_item_update(self->parent);
      }
    }
    if (update_task_list_count > 0) update_task_list_count--;
    if (update_task_list_count > 0) break;
    if (!self->lock) errands_task_list_item_save(self->list);
    errands_sidebar_update_filter_rows();
    errands_sidebar_task_list_update_counter(self->list->uid);
    errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_MORE_STRICT);
    errands_task_list_filter(state.main_window->task_list, GTK_FILTER_CHANGE_MORE_STRICT);
  } break;

  case PROP_COLOR: {
    const char *new_color = g_value_get_string(value);
    if (g_strcmp0(self->color, new_color) == 0) break;
    g_clear_pointer(&self->color, g_free);
    self->color = g_strdup(new_color);
    errands_data_set_color(self->ical, self->color);
    if (!self->lock) errands_task_list_item_save(self->list);
  } break;

  case PROP_PRIORITY: {
    gint new_priority = CLAMP(g_value_get_int(value), 0, 10);
    if (new_priority == self->priority) break;
    self->priority = new_priority;
    errands_data_set_priority(self->ical, self->priority);
    if (!self->lock) errands_task_list_item_save(self->list);
  } break;

  case PROP_NOTES: {
    const char *new_notes = g_value_get_string(value);
    const char *normalized = (new_notes && *new_notes) ? new_notes : NULL;
    if (g_strcmp0(self->notes, normalized) == 0) break;
    g_clear_pointer(&self->notes, g_free);
    self->notes = g_strdup(normalized);
    errands_data_set_notes(self->ical, self->notes);
    update_search_blob(self);
    if (!self->lock) errands_task_list_item_save(self->list);
  } break;

  case PROP_DTSTART: {
    icaltimetype *new_time_p = g_value_get_pointer(value);
    icaltimetype new_time = *new_time_p;
    if (icaltime_compare(new_time, self->dtstart) == 0) break;
    self->dtstart = new_time;
    errands_data_set_start(self->ical, self->dtstart);
    if (!self->lock) errands_task_list_item_save(self->list);
  } break;

  case PROP_DTEND: {
    icaltimetype *new_time_p = g_value_get_pointer(value);
    icaltimetype new_time = *new_time_p;
    if (icaltime_compare(new_time, self->dtend) == 0) break;
    self->dtend = new_time;
    errands_data_set_due(self->ical, self->dtend);
    if (!self->lock) errands_task_list_item_save(self->list);
  } break;

  case PROP_TAGS: {
    const GStrv new_tags = g_value_get_pointer(value);
    if (!self->tags && !new_tags) break;
    if (self->tags && !new_tags) {
      g_clear_pointer(&self->tags, g_strfreev);
      errands_data_set_tags(self->ical, NULL);
      if (!self->lock) errands_task_list_item_save(self->list);
      break;
    }
    if (new_tags && self->tags && g_strv_equal((const gchar *const *)self->tags, (const gchar *const *)new_tags)) break;
    g_clear_pointer(&self->tags, g_strfreev);
    self->tags = g_strdupv(new_tags);
    errands_data_set_tags(self->ical, new_tags);
    update_search_blob(self);
    if (!self->lock) errands_task_list_item_save(self->list);
  } break;

  case PROP_ATTACHMENTS: {
    const GStrv new_attachments = g_value_get_pointer(value);
    if (!self->attachments && !new_attachments) break;
    if (self->attachments && !new_attachments) {
      g_clear_pointer(&self->attachments, g_strfreev);
      errands_data_set_attachments(self->ical, NULL);
      if (!self->lock) errands_task_list_item_save(self->list);
      break;
    }
    if (new_attachments && self->attachments &&
        g_strv_equal((const gchar *const *)self->attachments, (const gchar *const *)new_attachments))
      break;
    g_clear_pointer(&self->attachments, g_strfreev);
    self->attachments = g_strdupv(new_attachments);
    errands_data_set_attachments(self->ical, new_attachments);
    if (!self->lock) errands_task_list_item_save(self->list);
  } break;

  case PROP_RRULE: {
    const struct icalrecurrencetype *new_rrule = g_value_get_pointer(value);
    if (!new_rrule) {
      if (self->rrule) icalrecurrencetype_unref(self->rrule);
      errands_data_set_rrule(self->ical, NULL);
      break;
    }
    if (icalrecurrence_compare(self->rrule, new_rrule)) break;
    if (self->rrule) icalrecurrencetype_unref(self->rrule);
    if (new_rrule->freq != ICAL_NO_RECURRENCE) self->rrule = icalrecurrencetype_clone((struct icalrecurrencetype *)new_rrule);
    errands_data_set_rrule(self->ical, self->rrule);
    if (!self->lock) errands_task_list_item_save(self->list);
  } break;

  case PROP_SEARCH_MATCHED: self->search_matched = g_value_get_boolean(value); break;

  case PROP_UNCOMPLETED_COUNT: {
    g_clear_pointer(&self->uncompleted_count, g_free);
    self->uncompleted_count = g_strdup(g_value_get_string(value));
  } break;

  case PROP_HAS_NO_CHILDREN: self->has_no_children = g_value_get_boolean(value); break;

  case PROP_ICAL: {
    icalcomponent *new_ical = g_value_get_pointer(value);
    if (!new_ical) {
      g_warning("Task item received a NULL iCalendar component");
      break;
    }
    self->ical = new_ical;
    gboolean old_lock = self->lock;
    self->lock = TRUE;
    errands_task_item_set_title(self, errands_data_get_text(self->ical));
    errands_task_item_set_color(self, errands_data_get_color(self->ical));
    errands_task_item_set_notes(self, errands_data_get_notes(self->ical));
    errands_task_item_set_completed(self, errands_data_is_completed(self->ical));
    errands_task_item_set_cancelled(self, errands_data_get_cancelled(self->ical));
    errands_task_item_set_priority(self, errands_data_get_priority(self->ical));
    icaltimetype dtstart = errands_data_get_start(self->ical);
    icaltimetype dtend = errands_data_get_due(self->ical);
    errands_task_item_set_dtstart(self, &dtstart);
    errands_task_item_set_dtend(self, &dtend);
    g_auto(GStrv) tags = errands_data_get_tags(self->ical);
    errands_task_item_set_tags(self, tags);
    g_auto(GStrv) atts = errands_data_get_attachments(self->ical);
    errands_task_item_set_attachments(self, atts);
    struct icalrecurrencetype *rrule = errands_data_get_rrule(self->ical);
    errands_task_item_set_rrule(self, rrule);
    icalrecurrencetype_unref(rrule);
    self->lock = old_lock;
    update_search_blob(self);
    errands_task_item_update(self);
  } break;

  case PROP_TASK_WIDGET: self->task_widget = g_value_get_object(value); break;

  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void finalize(GObject *object) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);

  if (self->rrule) icalrecurrencetype_unref(self->rrule);
  g_clear_object(&self->children_model);
  g_clear_pointer(&self->uid, g_free);
  g_clear_pointer(&self->title, g_free);
  g_clear_pointer(&self->color, g_free);
  g_clear_pointer(&self->notes, g_free);
  g_clear_pointer(&self->tags, g_strfreev);
  g_clear_pointer(&self->attachments, g_strfreev);
  g_clear_pointer(&self->search_blob, g_free);
  g_clear_pointer(&self->uncompleted_count, g_free);

  G_OBJECT_CLASS(errands_task_item_parent_class)->finalize(object);
}

static void errands_task_item_class_init(ErrandsTaskItemClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  obj_properties[PROP_UID] = g_param_spec_string("uid", "UID", "UID of the task", NULL, G_PARAM_READABLE);
  obj_properties[PROP_TITLE] = g_param_spec_string("title", "Title", "Title of the task", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_COMPLETED] =
      g_param_spec_boolean("completed", "Completed", "Whether the task is completed", false, G_PARAM_READWRITE);
  obj_properties[PROP_CANCELLED] =
      g_param_spec_boolean("cancelled", "Cancelled", "Whether the task is cancelled", false, G_PARAM_READWRITE);
  obj_properties[PROP_COLOR] = g_param_spec_string("color", "Task Color", "Color of the task", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_PRIORITY] = g_param_spec_int("priority", "Priority", "Priority of the task", 0, 10, 0, G_PARAM_READWRITE);
  obj_properties[PROP_NOTES] = g_param_spec_string("notes", "Notes", "Notes of the task", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_DTSTART] = g_param_spec_pointer("dtstart", "DTStart", "Start time of the task", G_PARAM_READWRITE);
  obj_properties[PROP_DTEND] = g_param_spec_pointer("dtend", "DTEnd", "End time of the task", G_PARAM_READWRITE);
  obj_properties[PROP_TAGS] = g_param_spec_pointer("tags", "Tags", "Tags of the task", G_PARAM_READWRITE);
  obj_properties[PROP_ATTACHMENTS] =
      g_param_spec_pointer("attachments", "Attachments", "Attachments of the task", G_PARAM_READWRITE);
  obj_properties[PROP_RRULE] = g_param_spec_pointer("rrule", "RRule", "Recurrence rule of the task", G_PARAM_READWRITE);
  obj_properties[PROP_SEARCH_BLOB] =
      g_param_spec_string("search-blob", "Search Blob", "Search blob of the task", NULL, G_PARAM_READABLE);
  obj_properties[PROP_SEARCH_MATCHED] =
      g_param_spec_boolean("search-matched", "Search Mathed", "Task matched search", true, G_PARAM_READWRITE);
  obj_properties[PROP_UNCOMPLETED_COUNT] =
      g_param_spec_string("uncompleted-count", "Uncompleted Count", "Number of uncompleted subtasks", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_HAS_NO_CHILDREN] =
      g_param_spec_boolean("has-no-children", "Has No Children", "Whether the task has any children", true, G_PARAM_READWRITE);
  obj_properties[PROP_ICAL] =
      g_param_spec_pointer("ical", "ICAL Component", "ICAL component associated with the task.", G_PARAM_READWRITE);
  obj_properties[PROP_CHILDREN_MODEL] = g_param_spec_object("children-model", "Children Model", "Model containing child tasks.",
                                                            G_TYPE_LIST_MODEL, G_PARAM_READABLE);
  obj_properties[PROP_TASK_WIDGET] = g_param_spec_object("task-widget", "Task Widget", "Widget associated with the task item.",
                                                         ERRANDS_TYPE_TASK, G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void errands_task_item_init(ErrandsTaskItem *self) {
  g_signal_connect_swapped(state.settings, "notify::tags", G_CALLBACK(on_global_tags_changed_cb), self);
  g_signal_connect_swapped(state.settings, "notify::show-completed", G_CALLBACK(errands_task_item_update), self);
  g_signal_connect_swapped(state.settings, "notify::show-cancelled", G_CALLBACK(errands_task_item_update), self);
}

// --- PRIVATE --- //

static void update_search_blob(ErrandsTaskItem *self) {
  g_assert_nonnull(self);

  g_clear_pointer(&self->search_blob, g_free);
  g_autoptr(GString) str = g_string_new(NULL);
  g_string_printf(str, "%s\n%s\n", self->title ? self->title : "", self->notes ? self->notes : "");
  if (self->tags) for_range(i, 0, g_strv_length(self->tags)) g_string_append_printf(str, "%s\n", self->tags[i]);
  self->search_blob = g_utf8_casefold(str->str, -1);
}

static void on_global_tags_changed_cb(ErrandsTaskItem *self) {
  const GStrv g_tags = errands_settings_get_tags(state.settings);
  g_auto(GStrv) old_tags = errands_data_get_tags(self->ical);
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for_range(i, 0, g_strv_length(g_tags)) {
    const gchar *tag = g_tags[i];
    bool contains = old_tags && g_strv_contains((const gchar *const *)old_tags, tag);
    if (contains) g_strv_builder_add(builder, tag);
  }
  g_auto(GStrv) new_tags = g_strv_builder_end(builder);
  if (g_strv_hash(g_tags) != g_strv_hash(old_tags)) errands_task_item_set_tags(self, new_tags);
}

static inline bool item_is_descendant_of(ErrandsTaskItem *item, ErrandsTaskItem *tagret) {
  if (!item || !tagret) return false;
  ErrandsTaskItem *parent = tagret->parent;
  while (parent) {
    if (parent == item) return true;
    parent = parent->parent;
  }
  return false;
}

static void clone_task_tree(ErrandsTaskItem *self, ErrandsTaskListItem *new_list, const char *new_parent_uid) {
  icalcomponent *clone = icalcomponent_clone(self->ical);
  errands_data_set_deleted(clone, false);
  errands_data_set_synced(clone, false);
  if (new_parent_uid) errands_data_set_parent(clone, new_parent_uid);
  icalcomponent_add_component(new_list->ical, clone);
  GListModel *children = G_LIST_MODEL(self->children_model);
  for (guint i = 0; i < g_list_model_get_n_items(children); i++) {
    g_autoptr(ErrandsTaskItem) child = g_list_model_get_item(children, i);
    clone_task_tree(child, new_list, NULL);
  }
}

static inline icalcomponent *find_icalcomponent_by_uid(ErrandsTaskListItem *list, const char *uid) {
  if (!list || !uid) return NULL;

  g_autoptr(GPtrArray) tasks = vcalendar_to_array(list->ical);
  for_range(i, 0, tasks->len) {
    icalcomponent *task = g_ptr_array_index(tasks, i);
    const char *task_uid = errands_data_get_uid(task);
    if (g_str_equal(uid, task_uid)) return task;
  }
  return NULL;
}

// --- PUBLIC --- //

ErrandsTaskItem *errands_task_item_new(icalcomponent *ical, ErrandsTaskListItem *list, ErrandsTaskItem *parent) {
  g_assert_nonnull(ical);
  g_assert_nonnull(list);

  ErrandsTaskItem *self = g_object_new(ERRANDS_TYPE_TASK_ITEM, NULL);

  self->uid = g_strdup(errands_data_get_uid(ical));
  self->title = g_strdup(errands_data_get_text(ical));
  self->color = g_strdup(errands_data_get_color(ical));
  self->notes = g_strdup(errands_data_get_notes(ical));
  self->completed = errands_data_is_completed(ical);
  self->cancelled = errands_data_get_cancelled(ical);
  self->priority = errands_data_get_priority(ical);
  self->dtstart = errands_data_get_start(ical);
  self->dtend = errands_data_get_due(ical);
  self->tags = errands_data_get_tags(ical);
  self->attachments = errands_data_get_attachments(ical);
  self->rrule = errands_data_get_rrule(ical);
  self->ical = ical;

  self->parent = parent;
  self->list = list;
  self->search_matched = true;

  self->children_model = g_list_store_new(ERRANDS_TYPE_TASK_ITEM);
  g_autoptr(GPtrArray) vtodos = vcalendar_to_array(list->ical);
  for (guint i = 0; i < vtodos->len; ++i) {
    icalcomponent *c = g_ptr_array_index(vtodos, i);
    const char *parent = errands_data_get_parent(c);
    if (errands_data_get_deleted(c) || !parent || !g_str_equal(parent, self->uid)) continue;
    g_autoptr(ErrandsTaskItem) item = errands_task_item_new(c, self->list, self);
    g_list_store_append(self->children_model, item);
  }

  update_search_blob(self);
  errands_task_item_update(self);

  if (!errands_data_get_synced(self->ical)) errands_sync_push_task(self);

  return self;
}

void errands_task_item_update(ErrandsTaskItem *self) {
  g_assert_nonnull(self);

  bool show_completed = errands_settings_get_show_completed(state.settings);
  bool show_cancelled = errands_settings_get_show_cancelled(state.settings);
  GListModel *children_model = G_LIST_MODEL(self->children_model);
  gint visible = 0, uncompleted = 0;
  for_range(i, 0, g_list_model_get_n_items(children_model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(children_model, i);
    if (errands_data_get_deleted(item->ical)) continue;
    if (!item->completed && !item->cancelled) uncompleted++;
    if ((!show_completed && item->completed) || (!show_cancelled && item->cancelled)) continue;
    visible++;
  }
  g_object_set(self, "uncompleted-count", uncompleted > 0 ? tmp_str_printf("%d", uncompleted) : "", "has-no-children",
               visible == 0, NULL);
}

void errands_task_item_add_tag(ErrandsTaskItem *self, const char *tag) {
  g_assert_nonnull(self);
  g_assert_nonnull(tag);

  icalcomponent_add_property(self->ical, icalproperty_new_categories(tag));
  errands_data_set_synced(self->ical, false);
  errands_data_set_changed(self->ical, icaltime_now());
  g_auto(GStrv) tags = errands_data_get_tags(self->ical);
  errands_task_item_set_tags(self, tags);
}

void errands_task_item_remove_tag(ErrandsTaskItem *self, const char *tag) {
  g_assert_nonnull(self);
  g_assert_nonnull(tag);

  errands_data_remove_tag(self->ical, tag);
  g_auto(GStrv) tags = errands_data_get_tags(self->ical);
  errands_task_item_set_tags(self, tags);
}

void errands_task_item_delete(ErrandsTaskItem *self) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_ITEM(self));

  errands_data_set_deleted(self->ical, true);
  errands_sync_push_task(self);
  GListModel *model = G_LIST_MODEL(self->children_model);
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    errands_task_item_delete(item);
  }
  if (!self->parent) g_list_store_remove_data(self->list->tasks, self);
  else g_list_store_remove_data(self->parent->children_model, self);
}

int errands_task_item_delete_completed(ErrandsTaskItem *self) {
  g_return_val_if_fail(self, 0);

  GListModel *model = G_LIST_MODEL(self->children_model);
  int deleted_n = 0;
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    icalcomponent *ical = errands_task_item_get_ical(item);
    CONTINUE_IF(errands_data_is_completed(ical));
    errands_data_set_deleted(ical, true);
    deleted_n += errands_task_item_delete_cancelled(item);
  }
  if (deleted_n > 0) errands_task_item_update(self);

  return deleted_n;
}

int errands_task_item_delete_cancelled(ErrandsTaskItem *self) {
  g_return_val_if_fail(self, 0);

  GListModel *model = G_LIST_MODEL(self->children_model);
  int deleted_n = 0;
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    icalcomponent *ical = errands_task_item_get_ical(item);
    CONTINUE_IF(errands_data_get_cancelled(ical));
    errands_data_set_deleted(ical, true);
    deleted_n += errands_task_item_delete_cancelled(item);
  }
  if (deleted_n > 0) errands_task_item_update(self);

  return deleted_n;
}

int errands_task_item_remove_deleted_tasks(ErrandsTaskItem *self) {
  g_return_val_if_fail(self, 0);

  bool sync_enabled = errands_settings_get_sync_enabled(state.settings);
  GListModel *model = G_LIST_MODEL(self->children_model);
  int deleted_n = 0;
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    deleted_n += errands_task_item_remove_deleted_tasks(item);
    icalcomponent *ical = errands_task_item_get_ical(item);
    bool deleted = errands_data_get_deleted(ical);
    bool synced = errands_data_get_synced(ical);
    if (deleted && (synced || !sync_enabled)) {
      icalcomponent_remove_component(self->list->ical, item->ical);
      g_list_store_remove(self->children_model, i--);
      deleted_n++;
    }
  }
  if (deleted_n > 0) errands_task_item_update(self);
  return deleted_n;
}

void errands_task_item_create_task(ErrandsTaskItem *parent, const char *text) {
  icalcomponent *ical = icalcomponent_new(ICAL_VTODO_COMPONENT);
  errands_data_set_text(ical, text);
  g_autofree gchar *uid = g_uuid_string_random();
  errands_data_set_uid(ical, uid);
  errands_data_set_created(ical, icaltime_now());
  errands_data_set_parent(ical, errands_data_get_uid(parent->ical));
  icalcomponent_add_component(parent->list->ical, ical);
  g_autoptr(ErrandsTaskItem) task = errands_task_item_new(ical, parent->list, parent);
  g_list_store_append(parent->children_model, task);
  errands_task_list_item_save(parent->list);
  errands_task_item_update(parent);
}

bool errands_task_item_export(ErrandsTaskItem *self, const char *path) {
  icalcomponent *ical = icalcomponent_new_vcalendar();
  icalcomponent_add_component(ical, icalcomponent_clone(self->ical));
  g_autoptr(GError) error = NULL;
  const char *contents = icalcomponent_as_ical_string(ical);
  if (!g_file_set_contents(path, contents, -1, &error)) {
    g_warning("Failed to write file %s", path);
    return false;
  }
  return true;
}

bool errands_task_item_is_due(ErrandsTaskItem *self) { return errands_data_is_due(self->ical); }

bool errands_task_item_move(ErrandsTaskItem *self, ErrandsTaskItem *parent, ErrandsTaskListItem *list) {
  g_assert_nonnull(self);
  g_assert_nonnull(list);

  if (parent == self || (parent && self->parent == parent)) return FALSE;
  if (parent && item_is_descendant_of(self, parent)) {
    errands_window_add_toast(tmp_str_printf(_("Can't make task a sub-task of itself.")), 5);
    return FALSE;
  }
  if (parent && errands_task_item_get_list(parent) != list) return FALSE;
  if (self->list == list && self->parent == parent) return FALSE;

  ErrandsTaskItem *old_parent = self->parent;
  GListStore *old_model = errands_task_item_get_parent_model(self);
  ErrandsTaskListItem *old_list = self->list;
  if (old_list != list) {
    g_autoptr(ErrandsTaskItem) old_task = g_object_ref(self);
    // Clone the entire hierarchy into the new calendar.
    clone_task_tree(self, list, parent ? parent->uid : NULL);
    // Delete old tasks (queues remote deletions recursively).
    errands_task_item_delete(self);
    // Create the new hierarchy (queues remote additions recursively).
    icalcomponent *clone = find_icalcomponent_by_uid(list, self->uid);
    g_assert_nonnull(clone);
    g_autoptr(ErrandsTaskItem) new_task = errands_task_item_new(clone, list, parent);
    g_list_store_append(parent ? parent->children_model : list->tasks, new_task);
  } else {
    errands_task_item_set_parent(self, parent);
    g_list_store_append(parent ? parent->children_model : list->tasks, self);
    g_list_store_remove_data(old_model, self);
  }
  errands_task_list_item_save(list);
  if (old_list != list) {
    errands_sidebar_task_list_update_counter(old_list->uid);
    errands_sidebar_task_list_update_counter(list->uid);
    errands_task_list_item_save(old_list);
  }
  if (parent) errands_task_item_update(parent);
  if (old_parent) errands_task_item_update(old_parent);
  if (state.main_window && state.main_window->task_list) errands_task_list_update(state.main_window->task_list);
  return TRUE;
}

// ---------- GETTERS ---------- //

ErrandsTaskListItem *errands_task_item_get_list(ErrandsTaskItem *self) { return self->list; }
const char *errands_task_item_get_uid(ErrandsTaskItem *self) { return self->uid; }
const char *errands_task_item_get_title(ErrandsTaskItem *self) { return self->title; }
icalcomponent *errands_task_item_get_ical(ErrandsTaskItem *self) { return self->ical; }
gint errands_task_item_get_priority(ErrandsTaskItem *self) { return self->priority; }
gboolean errands_task_item_get_completed(ErrandsTaskItem *self) { return self->completed; }
gboolean errands_task_item_get_cancelled(ErrandsTaskItem *self) { return self->cancelled; }
const char *errands_task_item_get_notes(ErrandsTaskItem *self) { return self->notes; }
const char *errands_task_item_get_search_blob(ErrandsTaskItem *self) { return self->search_blob; }
icaltimetype errands_task_item_get_dtstart(ErrandsTaskItem *self) { return self->dtstart; }
icaltimetype errands_task_item_get_dtend(ErrandsTaskItem *self) { return self->dtend; }
const GStrv errands_task_item_get_tags(ErrandsTaskItem *self) { return self->tags; }
const GStrv errands_task_item_get_attachments(ErrandsTaskItem *self) { return self->attachments; }
struct icalrecurrencetype *errands_task_item_get_rrule(ErrandsTaskItem *self) {
  return self->rrule ? icalrecurrencetype_clone(self->rrule) : NULL;
}
const char *errands_task_item_get_color(ErrandsTaskItem *self) { return self->color; }
ErrandsTaskItem *errands_task_item_get_parent(ErrandsTaskItem *self) { return self->parent; }
gboolean errands_task_item_get_search_matched(ErrandsTaskItem *self) { return self->search_matched; }

const char *errands_task_item_get_priority_as_string(ErrandsTaskItem *self) {
  gint priority = self->priority;
  if (priority > 0 && priority <= 3) return "high";
  if (priority > 3 && priority <= 6) return "medium";
  if (priority > 6) return "low";
  return "none";
}

const char *errands_task_item_get_priority_as_tstring(ErrandsTaskItem *self) {
  gint priority = self->priority;
  if (priority > 0 && priority <= 3) return C_("Priority", "High");
  if (priority > 3 && priority <= 6) return C_("Priority", "Medium");
  if (priority > 6) return C_("Priority", "Low");
  return C_("Priority", "None");
}

GListModel *errands_task_item_get_children_model(ErrandsTaskItem *self) { return G_LIST_MODEL(self->children_model); }

GListStore *errands_task_item_get_parent_model(ErrandsTaskItem *self) {
  return self->parent ? self->parent->children_model : self->list->tasks;
}

gboolean errands_task_item_get_synced(ErrandsTaskItem *self) { return errands_data_get_synced(self->ical); }

int errands_task_item_get_indent_level(ErrandsTaskItem *self) {
  int level = 0;
  ErrandsTaskItem *parent = self->parent;
  while (parent) {
    level++;
    parent = parent->parent;
  }
  return level;
}

gchar *errands_task_item_get_rrule_as_string(ErrandsTaskItem *self) {
  if (!self) return NULL;

  struct icalrecurrencetype *r = self->rrule;
  if (!r || r->freq == ICAL_NO_RECURRENCE) return NULL;

  GString *s = g_string_new(NULL);
  const int n = r->interval;
  switch (r->freq) {
  case ICAL_SECONDLY_RECURRENCE: g_string_append_printf(s, ngettext("Every second", "Every %d seconds", n), n); break;
  case ICAL_MINUTELY_RECURRENCE: g_string_append_printf(s, ngettext("Every minute", "Every %d minutes", n), n); break;
  case ICAL_HOURLY_RECURRENCE: g_string_append_printf(s, ngettext("Every hour", "Every %d hours", n), n); break;
  case ICAL_DAILY_RECURRENCE: g_string_append_printf(s, ngettext("Every day", "Every %d days", n), n); break;
  case ICAL_WEEKLY_RECURRENCE: g_string_append_printf(s, ngettext("Every week", "Every %d weeks", n), n); break;
  case ICAL_MONTHLY_RECURRENCE: g_string_append_printf(s, ngettext("Every month", "Every %d months", n), n); break;
  case ICAL_YEARLY_RECURRENCE: g_string_append_printf(s, ngettext("Every year", "Every %d years", n), n); break;
  case ICAL_NO_RECURRENCE: return NULL;
  }
  icaltimetype until = r->until;
  gint count = r->count;
  if (!icaltime_is_null_date(until)) {
    g_autoptr(GDateTime) d = g_date_time_new_from_unix_local(icaltime_as_timet(until));
    g_autofree gchar *ds = g_date_time_format(d, "%x");
    g_string_append_printf(s, _(" until %s"), ds);
  } else if (count > 0) g_string_append_printf(s, ngettext(" once", " %d times", count), count);
  return g_string_free(s, FALSE);
}

// --- SETTERS --- //

void errands_task_item_set_ical(ErrandsTaskItem *self, icalcomponent *ical) { g_object_set(self, "ical", ical, NULL); }
void errands_task_item_set_title(ErrandsTaskItem *self, const char *title) { g_object_set(self, "title", title, NULL); }
void errands_task_item_set_completed(ErrandsTaskItem *self, gboolean completed) {
  g_object_set(self, "completed", completed, NULL);
}
void errands_task_item_set_cancelled(ErrandsTaskItem *self, gboolean cancelled) {
  g_object_set(self, "cancelled", cancelled, NULL);
}
void errands_task_item_set_search_matched(ErrandsTaskItem *self, gboolean matched) {
  g_object_set(self, "search-matched", matched, NULL);
}
void errands_task_item_set_priority(ErrandsTaskItem *self, gint priority) { g_object_set(self, "priority", priority, NULL); }
void errands_task_item_set_priority_from_string(ErrandsTaskItem *self, const char *priority) {
  gint p = 0;
  if (!priority || g_str_equal(priority, "none")) p = 0;
  else if (g_str_equal(priority, "low")) p = 10;
  else if (g_str_equal(priority, "medium")) p = 6;
  else if (g_str_equal(priority, "high")) p = 1;
  g_object_set(self, "priority", p, NULL);
}
void errands_task_item_set_notes(ErrandsTaskItem *self, const char *notes) { g_object_set(self, "notes", notes, NULL); }
void errands_task_item_set_color(ErrandsTaskItem *self, const char *color) { g_object_set(self, "color", color, NULL); }
void errands_task_item_set_dtstart(ErrandsTaskItem *self, const icaltimetype *dtstart) {
  g_object_set(self, "dtstart", dtstart, NULL);
}
void errands_task_item_set_dtend(ErrandsTaskItem *self, const icaltimetype *dtend) { g_object_set(self, "dtend", dtend, NULL); }
void errands_task_item_set_tags(ErrandsTaskItem *self, const GStrv tags) { g_object_set(self, "tags", tags, NULL); }
void errands_task_item_set_attachments(ErrandsTaskItem *self, const GStrv attachments) {
  g_object_set(self, "attachments", attachments, NULL);
}
void errands_task_item_set_rrule(ErrandsTaskItem *self, const struct icalrecurrencetype *rrule) {
  g_object_set(self, "rrule", rrule, NULL);
}
void errands_task_item_set_parent(ErrandsTaskItem *self, ErrandsTaskItem *parent) {
  self->parent = parent;
  errands_data_set_parent(self->ical, parent ? parent->uid : NULL);
  errands_task_list_item_save(self->list);
}
void errands_task_item_set_list(ErrandsTaskItem *self, ErrandsTaskListItem *list) { self->list = list; }
