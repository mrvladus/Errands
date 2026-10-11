// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

/*

DESCRIPTION

Wrapper library for Evolution Data Server to manage CalDAV calendars and VTODOS.

----------------------------------------------------------------------

FUNCTIONALITY

Remote -> Local:
    [x] Calendars
        [x] Added
        [x] Modified
        [x] Removed
    [x] Tasks
        [x] Added
        [x] Modified
        [x] Removed

Local -> Remote:
    [x] Calendars
        [x] Add
        [x] Modify
        [x] Remove
    [x] Tasks
        [x] Add
        [x] Modify
        [x] Remove

----------------------------------------------------------------------

COMPILATION FLAGS

pkg-config --cflags --libs libedataserver-1.2 libecal-2.0

----------------------------------------------------------------------

EXAMPLE

----------------------------------------------------------------------

 */

#pragma once

#include <glib.h>
#include <libecal/libecal.h>
#include <libedataserver/libedataserver.h>

typedef struct _CalDAVCalendar CalDAVCalendar;
typedef struct _CalDAVClient CalDAVClient;

typedef struct {
  const char *url;
  const char *username;
  const char *password;

  void (*cal_added_cb)(CalDAVCalendar *c);
  void (*cal_modified_cb)(CalDAVCalendar *c, const char *name, const char *color);
  void (*cal_removed_cb)(const char *uid);
  void (*task_added_cb)(CalDAVCalendar *c, ICalComponent *task);
  void (*task_modified_cb)(CalDAVCalendar *c, ICalComponent *task);
  void (*task_removed_cb)(CalDAVCalendar *c, const char *uid);
} CalDAVClientConfig;

CalDAVClient *caldav_client_new(CalDAVClientConfig config);
gboolean caldav_client_poll(CalDAVClient *c);
CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *c, const gchar *name, const gchar *color);
gboolean caldav_client_remove_calendar(CalDAVCalendar *calendar);
void caldav_client_free(CalDAVClient *c);

const gchar *caldav_calendar_get_color(CalDAVCalendar *c);
const gchar *caldav_calendar_get_name(CalDAVCalendar *c);
const gchar *caldav_calendar_get_href(CalDAVCalendar *c);
const gchar *caldav_calendar_get_uid(CalDAVCalendar *c);
gboolean caldav_calendar_get_deleted(CalDAVCalendar *c);
gboolean caldav_calendar_set_color(CalDAVCalendar *c, const gchar *color);
gboolean caldav_calendar_set_name(CalDAVCalendar *c, const gchar *name);
gboolean caldav_calendar_set_properties(CalDAVCalendar *c, const gchar *name, const gchar *color);
gboolean caldav_calendar_create_task(CalDAVCalendar *c, ICalComponent *task);
gboolean caldav_calendar_update_task(CalDAVCalendar *c, ICalComponent *task);
gboolean caldav_calendar_remove_task(CalDAVCalendar *c, const char *uid);
void caldav_calendar_free(CalDAVCalendar *c);
