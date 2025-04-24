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

// --- GETTERS --- //

const char *errands_data_get_str(icalcomponent *data, DataPropStr prop) {
  const char *out = NULL;
  switch (prop) {
  case DATA_PROP_COLOR: out = get_x_prop_value(data, "X-ERRANDS-COLOR", "none"); break;
  case DATA_PROP_LIST_NAME: out = get_x_prop_value(data, "X-ERRANDS-LIST-NAME", ""); break;
  case DATA_PROP_LIST_UID: out = get_x_prop_value(data, "X-ERRANDS-LIST-UID", ""); break;
  case DATA_PROP_COMPLETED:
    out = icalproperty_as_ical_string(icalcomponent_get_first_property(data, ICAL_COMPLETED_PROPERTY));
    break;
  case DATA_PROP_CHANGED:
    out = icalproperty_as_ical_string(icalcomponent_get_first_property(data, ICAL_LASTMODIFIED_PROPERTY));
    break;
  case DATA_PROP_CREATED: out = icaltime_as_ical_string(icalcomponent_get_dtstamp(data)); break;
  case DATA_PROP_DUE: out = icaltime_as_ical_string(icalcomponent_get_due(data)); break;
  case DATA_PROP_END: out = icaltime_as_ical_string(icalcomponent_get_dtend(data)); break;
  case DATA_PROP_NOTES: out = icalcomponent_get_description(data); break;
  case DATA_PROP_PARENT:
    out = icalproperty_as_ical_string(icalcomponent_get_first_property(data, ICAL_RELATEDTO_PROPERTY));
    break;
  case DATA_PROP_RRULE:
    out = icalproperty_as_ical_string(icalcomponent_get_first_property(data, ICAL_RRULE_PROPERTY));
    break;
  case DATA_PROP_START: out = icaltime_as_ical_string(icalcomponent_get_dtstart(data)); break;
  case DATA_PROP_TEXT: out = icalcomponent_get_summary(data); break;
  case DATA_PROP_UID: out = icalcomponent_get_uid(data); break;
  }
  return out;
}

size_t errands_data_get_int(icalcomponent *data, DataPropInt prop) {
  size_t out = 0;
  switch (prop) {
  case DATA_PROP_PERCENT:
    out = atoi(icalproperty_as_ical_string(icalcomponent_get_first_property(data, ICAL_PERCENTCOMPLETE_PROPERTY)));
    break;
  case DATA_PROP_PRIORITY:
    out = atoi(icalproperty_as_ical_string(icalcomponent_get_first_property(data, ICAL_PRIORITY_PROPERTY)));
    break;
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
    if (value) {
      icalproperty *property = icalcomponent_get_first_property(data, ICAL_COMPLETED_PROPERTY);
      if (property) icalproperty_set_completed(property, icaltime_from_string(value));
      else icalcomponent_add_property(data, icalproperty_new_completed(icaltime_from_string(value)));
    } else {
      icalproperty *property = icalcomponent_get_first_property(data, ICAL_COMPLETED_PROPERTY);
      if (property) icalcomponent_remove_property(data, property);
    }
    break;
  }
  case DATA_PROP_CHANGED: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_LASTMODIFIED_PROPERTY);
    if (property) icalproperty_set_lastmodified(property, icaltime_from_string(value));
    else icalcomponent_add_property(data, icalproperty_new_lastmodified(icaltime_from_string(value)));
    break;
  }
  case DATA_PROP_CREATED: icalcomponent_set_dtstamp(data, icaltime_from_string(value)); break;
  case DATA_PROP_DUE: icalcomponent_set_due(data, icaltime_from_string(value)); break;
  case DATA_PROP_END: icalcomponent_set_dtend(data, icaltime_from_string(value)); break;
  case DATA_PROP_NOTES: icalcomponent_set_description(data, value); break;
  case DATA_PROP_PARENT: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_RELATEDTO_PROPERTY);
    if (property) icalproperty_set_relatedto(property, value);
    else icalcomponent_add_property(data, icalproperty_new_relatedto(value));
    break;
  }
  case DATA_PROP_RRULE: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_RRULE_PROPERTY);
    if (property) icalproperty_set_rrule(property, icalrecurrencetype_from_string(value));
    else icalcomponent_add_property(data, icalproperty_new_rrule(icalrecurrencetype_from_string(value)));
    break;
  }
  case DATA_PROP_START: icalcomponent_set_dtstart(data, icaltime_from_string(value)); break;
  case DATA_PROP_TEXT: icalcomponent_set_summary(data, value); break;
  case DATA_PROP_UID: icalcomponent_set_uid(data, value); break;
  }
}

void errands_data_set_int(icalcomponent *data, DataPropInt prop, size_t value) {
  switch (prop) {
  case DATA_PROP_PERCENT: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_PERCENTCOMPLETE_PROPERTY);
    if (property) icalproperty_set_percentcomplete(property, value);
    else icalcomponent_add_property(data, icalproperty_new_percentcomplete(value));
    break;
  }
  case DATA_PROP_PRIORITY: {
    icalproperty *property = icalcomponent_get_first_property(data, ICAL_PRIORITY_PROPERTY);
    if (property) icalproperty_set_priority(property, value);
    else icalcomponent_add_property(data, icalproperty_new_priority(value));
    break;
  }
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
    for (size_t i = 0; i < g_strv_length(value); i++)
      icalcomponent_add_property(data, icalproperty_new_categories(value[i]));
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
