#pragma once

#include "vendor/toolbox.h"

#include <gio/gio.h>
#include <glib.h>
#include <libical/ical.h>

extern GListStore *task_lists_model;

extern gchar *user_dir, *calendars_dir, *backups_dir;

// Initialize user data
void errands_data_init(void);
// Cleanup user data
void errands_data_cleanup(void);

bool errands_data_get_cancelled(icalcomponent *ical);
bool errands_data_get_deleted(icalcomponent *ical);
bool errands_data_get_notified(icalcomponent *ical);
bool errands_data_get_synced(icalcomponent *ical);
int errands_data_get_percent(icalcomponent *ical);
int errands_data_get_priority(icalcomponent *ical);
const char *errands_data_get_color(icalcomponent *ical);
const char *errands_data_get_list_name(icalcomponent *ical);
const char *errands_data_get_list_description(icalcomponent *ical);
const char *errands_data_get_notes(icalcomponent *ical);
const char *errands_data_get_parent(icalcomponent *ical);
const char *errands_data_get_text(icalcomponent *ical);
const char *errands_data_get_uid(icalcomponent *ical);
icaltimetype errands_data_get_changed(icalcomponent *ical);
icaltimetype errands_data_get_completed(icalcomponent *ical);
icaltimetype errands_data_get_created(icalcomponent *ical);
icaltimetype errands_data_get_due(icalcomponent *ical);
icaltimetype errands_data_get_end(icalcomponent *ical);
icaltimetype errands_data_get_start(icalcomponent *ical);
struct icalrecurrencetype *errands_data_get_rrule(icalcomponent *ical);
GStrv errands_data_get_attachments(icalcomponent *ical);
GStrv errands_data_get_tags(icalcomponent *ical);

void errands_data_set_cancelled(icalcomponent *ical, bool value);
void errands_data_set_deleted(icalcomponent *ical, bool value);
void errands_data_set_notified(icalcomponent *ical, bool value);
void errands_data_set_synced(icalcomponent *ical, bool value);
void errands_data_set_percent(icalcomponent *ical, int value);
void errands_data_set_priority(icalcomponent *ical, int value);
void errands_data_set_color(icalcomponent *ical, const char *value);
void errands_data_set_list_name(icalcomponent *ical, const char *value);
void errands_data_set_list_description(icalcomponent *ical, const char *value);
void errands_data_set_notes(icalcomponent *ical, const char *value);
void errands_data_set_parent(icalcomponent *ical, const char *value);
void errands_data_set_text(icalcomponent *ical, const char *value);
void errands_data_set_uid(icalcomponent *ical, const char *value);
void errands_data_set_changed(icalcomponent *ical, icaltimetype value);
void errands_data_set_completed(icalcomponent *ical, icaltimetype value);
void errands_data_set_created(icalcomponent *ical, icaltimetype value);
void errands_data_set_due(icalcomponent *ical, icaltimetype value);
void errands_data_set_end(icalcomponent *ical, icaltimetype value);
void errands_data_set_start(icalcomponent *ical, icaltimetype value);
// Sets the rrule on the ical component, returns true if the rrule was set
bool errands_data_set_rrule(icalcomponent *ical, struct icalrecurrencetype *value);
void errands_data_set_attachments(icalcomponent *ical, GStrv value);
void errands_data_set_tags(icalcomponent *ical, GStrv value);

bool errands_data_is_completed(icalcomponent *ical);
bool errands_data_is_due(icalcomponent *ical);
void errands_data_add_tag(icalcomponent *ical, const char *tag);
bool errands_data_remove_tag(icalcomponent *ical, const char *tag);
