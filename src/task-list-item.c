// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "task-list-item.h"
#include "data.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "settings.h"
#include "state.h"
#include "sync.h"
#include "task-item.h"
#include "utils.h"

#include <glib/gi18n.h>
#include <libical/ical.h>

G_DEFINE_TYPE(ErrandsTaskListItem, errands_task_list_item, G_TYPE_OBJECT)

enum {
  PROP_0,

  PROP_UID,
  PROP_TITLE,
  PROP_COLOR,
  PROP_COUNT,

  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsTaskListItem *self = ERRANDS_TASK_LIST_ITEM(object);
  switch (prop_id) {
  case PROP_UID: g_value_set_string(value, self->uid); break;
  case PROP_TITLE: g_value_set_string(value, self->title); break;
  case PROP_COLOR: g_value_set_boxed(value, &self->color); break;
  case PROP_COUNT: g_value_set_int(value, self->count); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsTaskListItem *self = ERRANDS_TASK_LIST_ITEM(object);
  switch (prop_id) {
  case PROP_UID: {
    const gchar *uid = g_value_get_string(value);
    if (g_strcmp0(self->uid, uid) == 0) break;
    g_clear_pointer(&self->uid, g_free);
    self->uid = g_strdup(uid);
    errands_data_set_uid(self->ical, self->uid);
    errands_task_list_item_save(self);
  } break;

  case PROP_TITLE: {
    const gchar *title = g_value_get_string(value);
    if (g_strcmp0(self->title, title) == 0) break;
    g_clear_pointer(&self->uid, g_free);
    self->title = g_strdup(title);
    errands_data_set_list_name(self->ical, self->title);
    errands_task_list_item_save(self);
  } break;

  case PROP_COLOR: {
    GdkRGBA *color = g_value_get_boxed(value);
    if (!color || gdk_rgba_equal(&self->color, color)) return;
    self->color = *color;
    errands_data_set_color(self->ical, gdk_rgba_to_str(&self->color));
    errands_task_list_item_save(self);
  } break;

  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void finalize(GObject *object) {
  ErrandsTaskListItem *self = ERRANDS_TASK_LIST_ITEM(object);

  g_list_store_remove_all(self->tasks);

  g_clear_object(&self->ical);
  g_clear_object(&self->tasks);
  g_clear_pointer(&self->uid, g_free);
  g_clear_pointer(&self->title, g_free);

  G_OBJECT_CLASS(errands_task_list_item_parent_class)->finalize(object);
}

static void errands_task_list_item_class_init(ErrandsTaskListItemClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  obj_properties[PROP_UID] =
      g_param_spec_pointer("uid", "Task List UID", "Unique identifier for the task list", G_PARAM_READWRITE);
  obj_properties[PROP_TITLE] = g_param_spec_string("title", "Task List Title", "Title of the task list", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_COLOR] =
      g_param_spec_boxed("color", "Task List Color", "Color of the task list", GDK_TYPE_RGBA, G_PARAM_READWRITE);
  obj_properties[PROP_COUNT] =
      g_param_spec_int("count", "Task List Count", "Number of uncompleted tasks", 0, G_MAXINT, 0, G_PARAM_READABLE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void errands_task_list_item_init(ErrandsTaskListItem *self) {}

// ---------- PUBLIC ---------- //

ErrandsTaskListItem *errands_task_list_item_new(ICalComponent *ical) {
  g_assert_nonnull(ical);

  ErrandsTaskListItem *self = g_object_new(ERRANDS_TYPE_TASK_LIST_ITEM, NULL);
  self->ical = g_object_ref(ical);
  self->uid = g_strdup(errands_data_get_uid(self->ical));
  self->title = g_strdup(errands_data_get_list_name(self->ical));
  const char *color = errands_data_get_color(self->ical);
  if (color) gdk_rgba_parse(&self->color, color);
  else self->color = (GdkRGBA){0, 0, 0, 0};
  self->tasks = g_list_store_new(ERRANDS_TYPE_TASK_ITEM);
  g_autoptr(GPtrArray) vtodos = vcalendar_to_array(ical);
  for (guint i = 0; i < vtodos->len; ++i) {
    g_autoptr(ICalComponent) c = g_ptr_array_index(vtodos, i);
    const char *parent = errands_data_get_parent(c);
    if (parent || errands_data_get_deleted(c)) continue;
    g_autoptr(ErrandsTaskItem) task = errands_task_item_new(c, self, NULL);
    g_list_store_append(self->tasks, task);
  }
  errands_task_list_item_update_count(self);

  return self;
}

ErrandsTaskListItem *errands_task_list_item_create(const char *uid, const char *name, const char *color, bool deleted,
                                                   bool synced) {
  ICalComponent *ical = i_cal_component_new_vcalendar();
  g_autofree gchar *gen_uid = g_uuid_string_random();
  errands_data_set_uid(ical, uid ? uid : gen_uid);
  errands_data_set_list_name(ical, name ? name : _("Untitled"));
  errands_data_set_color(ical, color);
  errands_data_set_deleted(ical, deleted);
  errands_data_set_synced(ical, synced);

  return errands_task_list_item_new(ical);
}

ErrandsTaskListItem *errands_task_list_item_load_from_ics(const char *path) {
  g_assert_nonnull(path);

  if (!g_str_has_suffix(path, ".ics")) {
    g_warning("File must be an .ics file");
    return NULL;
  }

  g_autofree gchar *uid = g_path_get_basename(path);
  char *dot = strrchr(uid, '.');
  if (dot) *dot = '\0';
  // Check if uid exists
  GListModel *model = G_LIST_MODEL(task_lists_model);
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskListItem) item = g_list_model_get_item(model, i);
    if (g_str_equal(item->uid, uid)) {
      g_warning("List already exists");
      return NULL;
    }
  }

  g_autofree gchar *contents = NULL;
  gsize length = 0;
  g_autoptr(GError) error = NULL;
  if (!g_file_get_contents(path, &contents, &length, &error)) {
    g_error("Failed to read file: %s", error->message);
    return NULL;
  }

  ICalComponent *ical = i_cal_component_new_from_string(contents);
  if (!ical) {
    g_warning("Failed to parse ical");
    return NULL;
  }

  const char *list_uid = errands_data_get_uid(ical);
  if (list_uid) errands_data_set_uid(ical, list_uid);
  const char *name = errands_data_get_list_name(ical);
  if (!name) errands_data_set_list_name(ical, "Untitled");
  const char *color = errands_data_get_color(ical);
  if (!color) errands_data_set_color(ical, generate_hex_as_str());

  return errands_task_list_item_new(ical);
}

void errands_task_list_item_update_count(ErrandsTaskListItem *self) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  gint total = 0, completed = 0;
  g_autoptr(GPtrArray) components = vcalendar_to_array(self->ical);
  for (guint i = 0; components && i < components->len; i++) {
    ICalComponent *c = g_ptr_array_index(components, i);
    if (errands_data_get_deleted(c) || errands_data_get_cancelled(c)) continue;
    if (errands_data_is_completed(c)) completed++;
    total++;
  }
  self->count = total - completed;
  g_object_notify(G_OBJECT(self), "count");
}

void errands_task_list_item_save(ErrandsTaskListItem *self) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  const char *path = tmp_str_printf("%s/%s.ics", calendars_dir, self->uid);
  g_autofree gchar *ical_str = i_cal_component_as_ical_string(self->ical);
  if (ical_str && !g_file_set_contents(path, ical_str, -1, NULL)) {
    g_message("Task List Item: Failed to save list '%s'", path);
    return;
  }
  g_message("Task List Item: Saved list '%s'", path);
}

void errands_task_list_item_delete(ErrandsTaskListItem *self) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  errands_data_set_deleted(self->ical, true);
  errands_task_list_item_save(self);
  errands_sync_delete_list(self);
  g_list_store_remove_data(task_lists_model, self);
}

int errands_task_list_item_delete_completed(ErrandsTaskListItem *self) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  GListModel *model = G_LIST_MODEL(self->tasks);
  int deleted_n = 0;
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    ICalComponent *ical = errands_task_item_get_ical(item);
    CONTINUE_IF(errands_data_is_completed(ical));
    errands_data_set_deleted(ical, true);
    errands_data_set_synced(ical, false);
    deleted_n += errands_task_item_delete_completed(item);
  }
  if (deleted_n > 0) {
    errands_task_list_item_update_count(self);
    errands_task_list_item_save(self);
  }

  return deleted_n;
}

int errands_task_list_item_delete_cancelled(ErrandsTaskListItem *self) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  GListModel *model = G_LIST_MODEL(self->tasks);
  int deleted_n = 0;
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    ICalComponent *ical = errands_task_item_get_ical(item);
    CONTINUE_IF(errands_data_get_cancelled(ical));
    errands_data_set_deleted(ical, true);
    errands_data_set_synced(ical, false);
    deleted_n += errands_task_item_delete_cancelled(item);
  }
  if (deleted_n > 0) {
    errands_task_list_item_update_count(self);
    errands_task_list_item_save(self);
  }

  return deleted_n;
}

void errands_task_list_item_remove_deleted_tasks(ErrandsTaskListItem *self) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  bool sync_enabled = errands_settings_get_sync_enabled(state.settings);
  GListModel *model = G_LIST_MODEL(self->tasks);
  int deleted_n = 0;
  for_range(i, 0, g_list_model_get_n_items(model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(model, i);
    deleted_n += errands_task_item_remove_deleted_tasks(item);
    ICalComponent *ical = errands_task_item_get_ical(item);
    bool deleted = errands_data_get_deleted(ical);
    bool synced = errands_data_get_synced(ical);
    if (deleted && (synced || !sync_enabled)) {
      i_cal_component_remove_component(self->ical, errands_task_item_get_ical(item));
      g_list_store_remove(self->tasks, i--);
      deleted_n++;
    }
  }

  g_autoptr(GPtrArray) components = vcalendar_to_array(self->ical);
  for (guint i = 0; components && i < components->len; i++) {
    ICalComponent *component = g_ptr_array_index(components, i);
    if (!errands_data_get_deleted(component) || !errands_data_get_synced(component)) continue;
    i_cal_component_remove_component(self->ical, component);
    deleted_n++;
  }

  if (deleted_n > 0) {
    errands_task_list_item_save(self);
    g_debug("Removed %d deleted tasks\n", deleted_n);
  }
}

struct ErrandsTaskItem *errands_task_list_item_create_task(ErrandsTaskListItem *self, struct ErrandsTaskItem *parent,
                                                           const char *text) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  g_autoptr(ICalComponent) ical = i_cal_component_new_vtodo();
  errands_data_set_text(ical, text);
  g_autofree gchar *uid = g_uuid_string_random();
  errands_data_set_uid(ical, uid);
  errands_data_set_created(ical, i_cal_time_get_date_time_now());
  i_cal_component_add_component(self->ical, ical);
  ErrandsTaskItem *task = errands_task_item_new(ical, self, NULL);
  g_list_store_append(self->tasks, task);
  errands_task_list_item_save(self);

  return (struct ErrandsTaskItem *)task;
}

// ---------- SETTERS ---------- //

void errands_task_list_item_set_title(ErrandsTaskListItem *self, const char *title) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  g_object_set(self, "title", title, NULL);
}

void errands_task_list_item_set_color(ErrandsTaskListItem *self, GdkRGBA *color) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  g_object_set(self, "color", color, NULL);
}

void errands_task_list_item_set_color_from_string(ErrandsTaskListItem *self, const char *color) {
  g_assert_nonnull(self);
  g_assert(ERRANDS_IS_TASK_LIST_ITEM(self));

  GdkRGBA rgba;
  gdk_rgba_parse(&rgba, color);
  g_object_set(self, "color", &rgba, NULL);
}
