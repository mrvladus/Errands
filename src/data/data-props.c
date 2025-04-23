#include "../utils.h"
#include "data.h"
#include <libical/ical.h>

// --- UTILS --- //

static icalproperty *get_x_prop(icalcomponent *ical, const char *xprop, const char *default_val) {
  icalproperty *property = icalcomponent_get_first_property(ical, ICAL_X_PROPERTY);
  while (property) {
    const char *name = icalproperty_get_x_name(property);
    if (name && !strcmp(name, xprop)) return property;
    property = icalcomponent_get_next_property(ical, ICAL_X_PROPERTY);
  }
  property = icalproperty_new_x(default_val);
  icalproperty_set_x_name(property, xprop);
  icalcomponent_add_property(ical, property);
  return property;
}

static const char *get_x_prop_value(icalcomponent *ical, const char *xprop, const char *default_val) {
  return icalproperty_get_value_as_string(get_x_prop(ical, xprop, default_val));
}

static void set_x_prop_value(icalcomponent *ical, const char *xprop, const char *val) {
  icalproperty *property = get_x_prop(ical, xprop, val);
  icalproperty_set_x(property, val);
}

static const char *get_prop_value(icalcomponent *ical, icalproperty_kind kind, const char *default_val) {
  icalproperty *property = icalcomponent_get_first_property(ical, kind);
  if (property) return icalproperty_get_value_as_string(property);
  if (default_val) {
    property = icalproperty_new(kind);
    icalproperty_set_value(property, icalvalue_new_from_string(ICAL_STRING_VALUE, default_val));
    icalcomponent_add_property(ical, property);
  }
  return default_val;
}

static void set_prop_value(icalcomponent *ical, icalproperty_kind kind, const char *val) {
  icalproperty *property = icalcomponent_get_first_property(ical, kind);
  if (!property) {
    property = icalproperty_new(kind);
    icalcomponent_add_property(ical, property);
  }
  icalproperty_set_value(property, icalvalue_new_from_string(ICAL_STRING_VALUE, val));
}

static void set_prop_value_int(icalcomponent *ical, icalproperty_kind kind, size_t val) {
  g_autofree gchar *str = g_strdup_printf("%zu", val);
  set_prop_value(ical, kind, str);
}

// --- GETTERS --- //

const char *errands_data_get_str(icalcomponent *data, DataPropStr prop) {
  const char *out = NULL;
  switch (prop) {
  case DATA_PROP_COLOR: out = get_x_prop_value(data, "X-ERRANDS-COLOR", ""); break;
  case DATA_PROP_LIST_NAME: out = get_x_prop_value(data, "X-ERRANDS-LIST-NAME", ""); break;
  case DATA_PROP_LIST_UID: out = get_x_prop_value(data, "X-ERRANDS-LIST-UID", ""); break;
  case DATA_PROP_COMPLETED: out = get_prop_value(data, ICAL_COMPLETED_PROPERTY, NULL); break;
  case DATA_PROP_CHANGED: out = get_prop_value(data, ICAL_LASTMODIFIED_PROPERTY, get_date_time()); break;
  case DATA_PROP_CREATED: out = get_prop_value(data, ICAL_DTSTAMP_PROPERTY, get_date_time()); break;
  case DATA_PROP_DUE: out = get_prop_value(data, ICAL_DUE_PROPERTY, NULL); break;
  case DATA_PROP_END: out = get_prop_value(data, ICAL_DTEND_PROPERTY, NULL); break;
  case DATA_PROP_NOTES: out = get_prop_value(data, ICAL_DESCRIPTION_PROPERTY, NULL); break;
  case DATA_PROP_PARENT: out = get_prop_value(data, ICAL_RELATEDTO_PROPERTY, NULL); break;
  case DATA_PROP_RRULE: out = get_prop_value(data, ICAL_RRULE_PROPERTY, NULL); break;
  case DATA_PROP_START: out = get_prop_value(data, ICAL_DTSTART_PROPERTY, NULL); break;
  case DATA_PROP_TEXT: out = get_prop_value(data, ICAL_SUMMARY_PROPERTY, ""); break;
  case DATA_PROP_UID: out = get_prop_value(data, ICAL_UID_PROPERTY, NULL); break;
  }
  return out;
}

size_t errands_data_get_int(icalcomponent *data, DataPropInt prop) {
  size_t out = 0;
  switch (prop) {
  case DATA_PROP_PERCENT: out = atoi(get_prop_value(data, ICAL_PERCENTCOMPLETE_PROPERTY, "0")); break;
  case DATA_PROP_PRIORITY: out = atoi(get_prop_value(data, ICAL_PRIORITY_PROPERTY, "0")); break;
  }
  return out;
}

bool errands_data_get_bool(icalcomponent *data, DataPropBool prop) {
  bool out = false;
  switch (prop) {
  case DATA_PROP_DELETED: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-DELETED", "0")); break;
  case DATA_PROP_EXPANDED: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-EXPANDED", "0")); break;
  case DATA_PROP_NOTIFIED: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-NOTIFIED", "0")); break;
  case DATA_PROP_TOOLBAR_SHOWN: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-TOOLBAR-SHOWN", "0")); break;
  case DATA_PROP_TRASH: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-TRASH", "0")); break;
  case DATA_PROP_SYNCED: out = (bool)atoi(get_x_prop_value(data, "X-ERRANDS-SYNCED", "0")); break;
  }
  return out;
}

GStrv errands_data_get_strv(icalcomponent *data, DataPropStrv prop) {
  GStrv out = NULL;
  switch (prop) {
  case DATA_PROP_ATTACHMENTS: out = g_strsplit(get_x_prop_value(data, "X-ERRANDS-ATTACHMENTS", ""), ",", -1); break;
  case DATA_PROP_TAGS: {
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
    for (icalproperty *p = icalcomponent_get_first_property(data, ICAL_CATEGORIES_PROPERTY); p != 0;
         p = icalcomponent_get_next_property(data, ICAL_CATEGORIES_PROPERTY))
      g_strv_builder_add(builder, icalproperty_get_value_as_string(p));
    out = g_strv_builder_end(builder);
    break;
  }
  }
  return out;
}

// --- SETTERS --- //

void errands_data_set_str(icalcomponent *data, DataPropStr prop, const char *value) {
  switch (prop) {
  case DATA_PROP_COLOR: set_x_prop_value(data, "X-ERRANDS-COLOR", value); break;
  case DATA_PROP_LIST_NAME: set_x_prop_value(data, "X-ERRANDS-LIST-NAME", value); break;
  case DATA_PROP_LIST_UID: set_x_prop_value(data, "X-ERRANDS-LIST-UID", value); break;
  case DATA_PROP_COMPLETED: {
    if (value) set_prop_value(data, ICAL_COMPLETED_PROPERTY, value);
    else {
      icalproperty *property = icalcomponent_get_first_property(data, ICAL_COMPLETED_PROPERTY);
      if (property) icalcomponent_remove_property(data, property);
    }
    break;
  }
  case DATA_PROP_CHANGED: set_prop_value(data, ICAL_LASTMODIFIED_PROPERTY, value); break;
  case DATA_PROP_CREATED: set_prop_value(data, ICAL_DTSTAMP_PROPERTY, value); break;
  case DATA_PROP_DUE: set_prop_value(data, ICAL_DUE_PROPERTY, value); break;
  case DATA_PROP_END: set_prop_value(data, ICAL_DTEND_PROPERTY, value); break;
  case DATA_PROP_NOTES: set_prop_value(data, ICAL_DESCRIPTION_PROPERTY, value); break;
  case DATA_PROP_PARENT: set_prop_value(data, ICAL_RELATEDTO_PROPERTY, value); break;
  case DATA_PROP_RRULE: set_prop_value(data, ICAL_RRULE_PROPERTY, value); break;
  case DATA_PROP_START: set_prop_value(data, ICAL_DTSTART_PROPERTY, value); break;
  case DATA_PROP_TEXT: set_prop_value(data, ICAL_SUMMARY_PROPERTY, value); break;
  case DATA_PROP_UID: set_prop_value(data, ICAL_UID_PROPERTY, value); break;
  }
}

void errands_data_set_int(icalcomponent *data, DataPropInt prop, size_t value) {
  switch (prop) {
  case DATA_PROP_PERCENT: set_prop_value_int(data, ICAL_PERCENTCOMPLETE_PROPERTY, value); break;
  case DATA_PROP_PRIORITY: set_prop_value_int(data, ICAL_PRIORITY_PROPERTY, value); break;
  }
}

void errands_data_set_bool(icalcomponent *data, DataPropBool prop, bool value) {
  const char *str = value ? "1" : "0";
  switch (prop) {
  case DATA_PROP_DELETED: set_x_prop_value(data, "X-ERRANDS-DELETED", str); break;
  case DATA_PROP_EXPANDED: set_x_prop_value(data, "X-ERRANDS-EXPANDED", str); break;
  case DATA_PROP_NOTIFIED: set_x_prop_value(data, "X-ERRANDS-NOTIFIED", str); break;
  case DATA_PROP_TOOLBAR_SHOWN: set_x_prop_value(data, "X-ERRANDS-TOOLBAR-SHOWN", str); break;
  case DATA_PROP_TRASH: set_x_prop_value(data, "X-ERRANDS-TRASH", str); break;
  case DATA_PROP_SYNCED: set_x_prop_value(data, "X-ERRANDS-SYNCED", str); break;
  }
}

void errands_data_set_strv(icalcomponent *data, DataPropStrv prop, GStrv value) {
  g_autofree gchar *str = g_strjoinv(",", value);
  switch (prop) {
  case DATA_PROP_ATTACHMENTS: set_x_prop_value(data, "X-ERRANDS-ATTACHMENTS", str); break;
  case DATA_PROP_TAGS:
    for (icalproperty *p = icalcomponent_get_first_property(data, ICAL_CATEGORIES_PROPERTY); p != 0;
         p = icalcomponent_get_next_property(data, ICAL_CATEGORIES_PROPERTY))
      icalcomponent_remove_property(data, p);
    set_prop_value(data, ICAL_CATEGORIES_PROPERTY, str);
    break;
  }
}

void errands_data_add_tag(icalcomponent *data, DataPropStrv prop, const char *tag) {
  switch (prop) {
  case DATA_PROP_TAGS: icalcomponent_add_property(data, icalproperty_new_categories(tag)); break;
  case DATA_PROP_ATTACHMENTS: break;
  }
}

void errands_data_remove_tag(icalcomponent *data, DataPropStrv prop, const char *tag) {
  switch (prop) {
  case DATA_PROP_TAGS:
    for (icalproperty *p = icalcomponent_get_first_property(data, ICAL_CATEGORIES_PROPERTY); p != 0;
         p = icalcomponent_get_next_property(data, ICAL_CATEGORIES_PROPERTY))
      if (!strcmp(tag, icalproperty_get_value_as_string(p))) icalcomponent_remove_property(data, p);
    break;
  case DATA_PROP_ATTACHMENTS: break;
  }
}
