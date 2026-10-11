#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <libical-glib/libical-glib.h>

extern GListStore *task_lists_model;

extern gchar *user_dir, *calendars_dir;

// Initialize user data
void errands_data_init(void);
// Cleanup user data
void errands_data_cleanup(void);

bool errands_data_get_cancelled(ICalComponent *ical);
bool errands_data_get_deleted(ICalComponent *ical);
bool errands_data_get_notified(ICalComponent *ical);
bool errands_data_get_synced(ICalComponent *ical);
int errands_data_get_percent(ICalComponent *ical);
int errands_data_get_priority(ICalComponent *ical);
const char *errands_data_get_color(ICalComponent *ical);
const char *errands_data_get_list_name(ICalComponent *ical);
const char *errands_data_get_list_description(ICalComponent *ical);
const char *errands_data_get_notes(ICalComponent *ical);
const char *errands_data_get_parent(ICalComponent *ical);
const char *errands_data_get_text(ICalComponent *ical);
const char *errands_data_get_uid(ICalComponent *ical);
ICalTime *errands_data_get_changed(ICalComponent *ical);
ICalTime *errands_data_get_completed(ICalComponent *ical);
ICalTime *errands_data_get_created(ICalComponent *ical);
ICalTime *errands_data_get_due(ICalComponent *ical);
ICalTime *errands_data_get_end(ICalComponent *ical);
ICalTime *errands_data_get_start(ICalComponent *ical);
ICalRecurrence *errands_data_get_rrule(ICalComponent *ical);
GStrv errands_data_get_attachments(ICalComponent *ical);
GStrv errands_data_get_tags(ICalComponent *ical);

void errands_data_set_cancelled(ICalComponent *ical, bool value);
void errands_data_set_deleted(ICalComponent *ical, bool value);
void errands_data_set_notified(ICalComponent *ical, bool value);
void errands_data_set_synced(ICalComponent *ical, bool value);
void errands_data_set_percent(ICalComponent *ical, int value);
void errands_data_set_priority(ICalComponent *ical, int value);
void errands_data_set_color(ICalComponent *ical, const char *value);
void errands_data_set_list_name(ICalComponent *ical, const char *value);
void errands_data_set_list_description(ICalComponent *ical, const char *value);
void errands_data_set_notes(ICalComponent *ical, const char *value);
void errands_data_set_parent(ICalComponent *ical, const char *value);
void errands_data_set_text(ICalComponent *ical, const char *value);
void errands_data_set_uid(ICalComponent *ical, const char *value);
void errands_data_set_changed(ICalComponent *ical, ICalTime *value);
void errands_data_set_completed(ICalComponent *ical, ICalTime *value);
// Takes ownership of value. Ref it if needed.
void errands_data_set_created(ICalComponent *ical, ICalTime *value);
// Takes ownership of value. Ref it if needed.
void errands_data_set_due(ICalComponent *ical, ICalTime *value);
// Takes ownership of value. Ref it if needed.
void errands_data_set_end(ICalComponent *ical, ICalTime *value);
// Takes ownership of value. Ref it if needed.
void errands_data_set_start(ICalComponent *ical, ICalTime *value);
// Sets the rrule on the ical component, returns true if the rrule was set
bool errands_data_set_rrule(ICalComponent *ical, ICalRecurrence *value);
void errands_data_set_attachments(ICalComponent *ical, GStrv value);
void errands_data_set_tags(ICalComponent *ical, GStrv value);

bool errands_data_is_completed(ICalComponent *ical);
bool errands_data_is_due(ICalComponent *ical);
void errands_data_add_tag(ICalComponent *ical, const char *tag);
bool errands_data_remove_tag(ICalComponent *ical, const char *tag);
