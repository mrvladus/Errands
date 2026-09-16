#include "task-item.h"
#include "data.h"
#include "glib-object.h"
#include "glib/gi18n.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "task.h"
#include <libical/ical.h>

struct _ErrandsTaskItem {
  GObject parent_instance;

  // Properties
  const char *title;
  gboolean completed;
  gboolean cancelled;
  const char *color;
  gint priority;
  const char *notes;
  icaltimetype dtstart;
  icaltimetype dtend;
  GStrv tags;
  GStrv attachments;

  const char *uncompleted_count; // The number of uncompleted subtasks
  gboolean has_no_children;      // If the task can be expanded (has any children)

  TaskData *data;
  ErrandsTaskItem *parent;
  GListStore *children_model;

  ErrandsTask *task_widget;
};

G_DEFINE_TYPE(ErrandsTaskItem, errands_task_item, G_TYPE_OBJECT)

enum {
  PROP_0,

  PROP_TITLE,
  PROP_COMPLETED,
  PROP_CANCELLED,
  PROP_COLOR,
  PROP_PRIORITY,
  PROP_NOTES,
  PROP_DTSTART,
  PROP_DTEND,
  PROP_TAGS,
  PROP_ATTACHMENTS,

  PROP_UNCOMPLETED_COUNT,
  PROP_HAS_NO_CHILDREN,

  PROP_DATA,
  PROP_CHILDREN_MODEL,
  PROP_TASK_WIDGET,
  N_PROPERTIES,
};

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL};
static int update_task_list_count = 0;

static void get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);
  switch (prop_id) {
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

  case PROP_UNCOMPLETED_COUNT: g_value_set_string(value, self->uncompleted_count); break;
  case PROP_HAS_NO_CHILDREN: g_value_set_boolean(value, self->has_no_children); break;

  case PROP_DATA: g_value_set_pointer(value, self->data); break;
  case PROP_CHILDREN_MODEL: g_value_set_object(value, self->children_model); break;

  case PROP_TASK_WIDGET: g_value_set_pointer(value, self->task_widget); break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);
  switch (prop_id) {
  case PROP_TITLE: {
    self->title = g_value_get_string(value);
    errands_data_set_text(self->data->ical, self->title);
    errands_list_data_save(self->data->list);
  } break;
  case PROP_COMPLETED: {
    gboolean old = self->completed;
    self->completed = g_value_get_boolean(value);
    if (old == self->completed) break;
    update_task_list_count++;
    errands_data_set_completed(self->data->ical, self->completed ? icaltime_get_date_time_now() : icaltime_null_time());
    // Complete all sub-tasks if the task is completed
    if (self->completed) {
      GListStore *sub_tasks = self->children_model;
      for (guint i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(sub_tasks)); i++) {
        ErrandsTaskItem *sub_task = g_list_model_get_item(G_LIST_MODEL(sub_tasks), i);
        g_object_set(sub_task, "completed", true, NULL);
      }
      errands_task_item_update(self);
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
    errands_list_data_save(self->data->list);
    errands_sidebar_update_filter_rows();
    errands_sidebar_task_list_update_counter(errands_data_get_uid(self->data->ical));
    errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_MORE_STRICT);
    errands_task_list_filter_tree(state.main_window->task_list, GTK_FILTER_CHANGE_MORE_STRICT);
  } break;
  case PROP_CANCELLED: {
    gboolean old = self->cancelled;
    self->cancelled = g_value_get_boolean(value);
    if (old == self->cancelled) break;
    update_task_list_count++;
    errands_data_set_cancelled(self->data->ical, self->cancelled);
    if (self->cancelled) {
      GListStore *sub_tasks = self->children_model;
      for (guint i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(sub_tasks)); i++) {
        ErrandsTaskItem *sub_task = g_list_model_get_item(G_LIST_MODEL(sub_tasks), i);
        g_object_set(sub_task, "cancelled", true, NULL);
      }
    } else {
      if (self->parent && self->parent->cancelled) g_object_set(self->parent, "cancelled", false, NULL);
    }
    if (update_task_list_count > 0) update_task_list_count--;
    if (update_task_list_count > 0) break;
    errands_list_data_save(self->data->list);
    errands_sidebar_update_filter_rows();
    errands_sidebar_task_list_update_counter(errands_data_get_uid(self->data->ical));
    errands_task_list_sort(state.main_window->task_list, GTK_SORTER_CHANGE_MORE_STRICT);
    errands_task_list_filter_tree(state.main_window->task_list, GTK_FILTER_CHANGE_MORE_STRICT);
  } break;
  case PROP_COLOR: {
    self->color = g_value_get_string(value);
    errands_data_set_color(self->data->ical, self->color);
    errands_list_data_save(self->data->list);
  } break;
  case PROP_PRIORITY: {
    self->priority = g_value_get_int(value);
    errands_data_set_priority(self->data->ical, self->priority);
    errands_list_data_save(self->data->list);
  } break;
  case PROP_NOTES: {
    self->notes = g_value_get_string(value);
    errands_data_set_notes(self->data->ical, self->notes);
    errands_list_data_save(self->data->list);
  } break;
  case PROP_DTSTART: {
    icaltimetype *dtstart = g_value_get_pointer(value);
    self->dtstart = *dtstart;
    errands_data_set_start(self->data->ical, self->dtstart);
    errands_list_data_save(self->data->list);
  } break;
  case PROP_DTEND: {
    icaltimetype *dtend = g_value_get_pointer(value);
    self->dtend = *dtend;
    errands_data_set_due(self->data->ical, self->dtend);
    errands_list_data_save(self->data->list);
  } break;
  case PROP_TAGS: {
    if (self->tags) g_strfreev(self->tags);
    self->tags = g_value_get_pointer(value);
  } break;
  case PROP_ATTACHMENTS: {
    if (self->attachments) g_strfreev(self->attachments);
    self->attachments = g_value_get_pointer(value);
  } break;

  case PROP_UNCOMPLETED_COUNT: self->uncompleted_count = g_value_get_string(value); break;
  case PROP_HAS_NO_CHILDREN: self->has_no_children = g_value_get_boolean(value); break;

  case PROP_DATA: self->data = g_value_get_pointer(value); break;
  case PROP_CHILDREN_MODEL: self->children_model = g_value_get_object(value); break;
  case PROP_TASK_WIDGET: {
    self->task_widget = g_value_get_pointer(value);
  } break;
  default: G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec); break;
  }
}

static void errands_task_item_dispose(GObject *object) {
  ErrandsTaskItem *self = ERRANDS_TASK_ITEM(object);
  if (self->children_model) g_object_unref(self->children_model);
  G_OBJECT_CLASS(errands_task_item_parent_class)->dispose(object);
}

static void errands_task_item_class_init(ErrandsTaskItemClass *klass) {
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = errands_task_item_dispose;

  object_class->get_property = get_property;
  object_class->set_property = set_property;

  obj_properties[PROP_TITLE] = g_param_spec_string("title", "Title", "Title of the task", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_COMPLETED] =
      g_param_spec_boolean("completed", "Completed", "Whether the task is completed", false, G_PARAM_READWRITE);
  obj_properties[PROP_CANCELLED] =
      g_param_spec_boolean("cancelled", "Cancelled", "Whether the task is cancelled", false, G_PARAM_READWRITE);
  obj_properties[PROP_COLOR] = g_param_spec_string("color", "Task Color", "Color of the task", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_PRIORITY] =
      g_param_spec_int("priority", "Priority", "Priority of the task", 0, 10, 0, G_PARAM_READWRITE);
  obj_properties[PROP_NOTES] = g_param_spec_string("notes", "Notes", "Notes of the task", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_DTSTART] =
      g_param_spec_pointer("dtstart", "DTStart", "Start time of the task", G_PARAM_READWRITE);
  obj_properties[PROP_DTEND] = g_param_spec_pointer("dtend", "DTEnd", "End time of the task", G_PARAM_READWRITE);
  obj_properties[PROP_TAGS] = g_param_spec_pointer("tags", "Tags", "Tags of the task", G_PARAM_READWRITE);
  obj_properties[PROP_ATTACHMENTS] =
      g_param_spec_pointer("attachments", "Attachments", "Attachments of the task", G_PARAM_READWRITE);

  obj_properties[PROP_UNCOMPLETED_COUNT] = g_param_spec_string(
      "uncompleted-count", "Uncompleted Count", "Number of uncompleted subtasks", NULL, G_PARAM_READWRITE);
  obj_properties[PROP_HAS_NO_CHILDREN] = g_param_spec_boolean(
      "has-no-children", "Has No Children", "Whether the task has any children", true, G_PARAM_READWRITE);

  obj_properties[PROP_DATA] =
      g_param_spec_pointer("data", "Task Data", "Data associated with the task.", G_PARAM_READWRITE);
  obj_properties[PROP_CHILDREN_MODEL] = g_param_spec_object(
      "children-model", "Children Model", "Model containing child tasks.", G_TYPE_LIST_MODEL, G_PARAM_READWRITE);
  obj_properties[PROP_TASK_WIDGET] =
      g_param_spec_pointer("task-widget", "Task Widget", "Widget associated with the task item.", G_PARAM_READWRITE);

  g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void errands_task_item_init(ErrandsTaskItem *self) {}

ErrandsTaskItem *errands_task_item_new(TaskData *data, ErrandsTaskItem *parent) {
  ErrandsTaskItem *self = g_object_new(ERRANDS_TYPE_TASK_ITEM, NULL);

  self->title = errands_data_get_text(data->ical);
  self->completed = errands_data_is_completed(data->ical);
  self->cancelled = errands_data_get_cancelled(data->ical);
  self->color = errands_data_get_color(data->ical);
  self->priority = errands_data_get_priority(data->ical);
  self->notes = errands_data_get_notes(data->ical);
  self->dtstart = errands_data_get_start(data->ical);
  self->dtend = errands_data_get_due(data->ical);
  self->tags = errands_data_get_tags(data->ical);
  self->attachments = errands_data_get_attachments(data->ical);
  errands_task_item_update(self);

  self->data = data;
  self->children_model = NULL;
  self->parent = parent;

  return self;
}

ErrandsTaskItem *errands_task_item_add_child(ErrandsTaskItem *self, TaskData *data) {
  if (!self || !data) return NULL;

  g_autoptr(ErrandsTaskItem) item = errands_task_item_new(data, self);
  g_list_store_append(self->children_model, item);
  errands_task_item_update(self);

  return item;
}

void errands_task_item_update(ErrandsTaskItem *self) {
  if (!self || !self->children_model) return;
  bool show_completed = errands_settings_get(SETTING_SHOW_COMPLETED).b;
  bool show_cancelled = errands_settings_get(SETTING_SHOW_CANCELLED).b;
  GListModel *children_model = G_LIST_MODEL(self->children_model);
  gint visible = 0;
  gint uncompleted = 0;
  for_range(i, 0, g_list_model_get_n_items(children_model)) {
    g_autoptr(ErrandsTaskItem) item = g_list_model_get_item(children_model, i);
    icalcomponent *ical = item->data->ical;
    bool deleted = errands_data_get_deleted(ical);
    bool completed = errands_data_is_completed(ical);
    bool cancelled = errands_data_get_cancelled(ical);
    if (deleted) continue;
    if (!completed && !cancelled) uncompleted++;
    if ((!show_completed && completed) || (!show_cancelled && cancelled)) continue;
    visible++;
  }
  g_object_set(self, "uncompleted-count", uncompleted > 0 ? tmp_str_printf("%d", uncompleted) : "", "has-no-children",
               visible == 0, NULL);
}

// ---------- PROPERTIES ---------- //

gint errands_task_item_get_priority(ErrandsTaskItem *self) { return self->priority; }

const char *errands_task_item_get_priority_as_string(ErrandsTaskItem *self) {
  gint priority = self->priority;
  if (priority == 0) return "none";
  if (priority > 0 && priority <= 3) return "low";
  if (priority > 3 && priority <= 6) return "medium";
  if (priority > 6) return "high";
  return "none";
}

const char *errands_task_item_get_priority_as_tstring(ErrandsTaskItem *self) {
  gint priority = self->priority;
  if (priority == 0) return C_("Priority", "None");
  if (priority > 0 && priority <= 3) return C_("Priority", "Low");
  if (priority > 3 && priority <= 6) return C_("Priority", "Medium");
  if (priority > 6) return C_("Priority", "High");
  return C_("Priority", "None");
}

const char *errands_task_item_get_notes(ErrandsTaskItem *self) { return self->notes; }

const char *errands_task_item_get_color(ErrandsTaskItem *self) {
  if (!self) return NULL;
  return self->color;
}

TaskData *errands_task_item_get_data(ErrandsTaskItem *self) {
  if (!self) return NULL;
  return self->data;
}

ErrandsTaskItem *errands_task_item_get_parent(ErrandsTaskItem *self) {
  if (!self) return NULL;
  return self->parent;
}

GListModel *errands_task_item_get_children_model(ErrandsTaskItem *self) {
  if (!self) return NULL;
  if (self->children_model) return G_LIST_MODEL(self->children_model);
  self->children_model = g_list_store_new(ERRANDS_TYPE_TASK_ITEM);
  for_range(i, 0, self->data->children->len) {
    TaskData *child = g_ptr_array_index(self->data->children, i);
    g_autoptr(ErrandsTaskItem) item = errands_task_item_new(child, self);
    g_list_store_append(self->children_model, item);
  }
  errands_task_item_update(self);
  return G_LIST_MODEL(self->children_model);
}

icaltimetype *errands_task_item_get_dtstart(ErrandsTaskItem *self) {
  if (!self) return NULL;
  return &self->dtstart;
}

icaltimetype *errands_task_item_get_dtend(ErrandsTaskItem *self) {
  if (!self) return NULL;
  return &self->dtend;
}

bool errands_task_item_is_due(ErrandsTaskItem *self) {
  if (!self) return false;
  return errands_data_is_due(self->data->ical);
}

// --- SETTERS --- //

void errands_task_item_set_priority(ErrandsTaskItem *self, gint priority) {
  g_object_set(self, "priority", priority, NULL);
}
void errands_task_item_set_priority_from_string(ErrandsTaskItem *self, const char *priority) {
  gint p = 0;
  if (!priority || g_str_equal(priority, "none")) p = 0;
  else if (g_str_equal(priority, "low")) p = 3;
  else if (g_str_equal(priority, "medium")) p = 6;
  else if (g_str_equal(priority, "high")) p = 10;
  g_object_set(self, "priority", p, NULL);
}
void errands_task_item_set_notes(ErrandsTaskItem *self, const char *notes) { g_object_set(self, "notes", notes, NULL); }
void errands_task_item_set_color(ErrandsTaskItem *self, const char *color) { g_object_set(self, "color", color, NULL); }
void errands_task_item_set_dtstart(ErrandsTaskItem *self, icaltimetype *dtstart) {
  g_object_set(self, "dtstart", dtstart, NULL);
}
void errands_task_item_set_dtend(ErrandsTaskItem *self, icaltimetype *dtend) {
  g_object_set(self, "dtend", dtend, NULL);
}
void errands_task_item_set_parent(ErrandsTaskItem *self, ErrandsTaskItem *parent) { self->parent = parent; }
