// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#pragma once

#include <glib-object.h>
#include <glib.h>
#include <libical/ical.h>

G_BEGIN_DECLS

// ---------- CalDAVClient ---------- //

#define CALDAV_TYPE_CLIENT (caldav_client_get_type())
G_DECLARE_FINAL_TYPE(CalDAVClient, caldav_client, CALDAV, CLIENT, GObject)

CalDAVClient *caldav_client_new(const gchar *base_url, const gchar *username, const gchar *password);
GPtrArray *caldav_client_pull_calendars(CalDAVClient *self);

const gchar *caldav_client_get_url(CalDAVClient *self);
const gchar *caldav_client_get_username(CalDAVClient *self);
const gchar *caldav_client_get_password(CalDAVClient *self);
const gchar *caldav_client_get_principal(CalDAVClient *self);
const gchar *caldav_client_get_home_set(CalDAVClient *self);

// ---------- CalDAVSyncResult ---------- //

typedef struct {
  GPtrArray *changed;      // CalDAVTodo *
  GPtrArray *deleted_urls; // gchar *
  gchar *sync_token;
} CalDAVSyncResult;

CalDAVSyncResult *caldav_sync_result_new(void);
void caldav_sync_result_free(CalDAVSyncResult *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CalDAVSyncResult, caldav_sync_result_free)

// ---------- CalDAVCalendarChanges ---------- //

typedef struct {
  GPtrArray *added;              // CalDAVCalendar *
  GPtrArray *deleted;            // CalDAVCalendar *
  GPtrArray *properties_changed; // CalDAVCalendar *
  GPtrArray *contents_changed;   // CalDAVCalendar *
} CalDAVCalendarChanges;

void caldav_calendar_changes_free(CalDAVCalendarChanges *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CalDAVCalendarChanges, caldav_calendar_changes_free)

// ---------- CalDAVCalendar ---------- //

#define CALDAV_TYPE_CALENDAR (caldav_calendar_get_type())
G_DECLARE_FINAL_TYPE(CalDAVCalendar, caldav_calendar, CALDAV, CALENDAR, GObject)

CalDAVCalendar *caldav_calendar_new(const gchar *url, const gchar *display_name, const gchar *color, const gchar *ctag);
CalDAVCalendar *caldav_calendar_create(CalDAVClient *c, const gchar *display_name, const gchar *color);
CalDAVSyncResult *caldav_calendar_sync(CalDAVClient *c, CalDAVCalendar *calendar);
gboolean caldav_calendar_delete(CalDAVClient *c, CalDAVCalendar *self);
gboolean caldav_calendar_update(CalDAVClient *c, CalDAVCalendar *self, const gchar *display_name, const gchar *color);
CalDAVCalendarChanges *caldav_calendar_compare(GPtrArray *old_calendars, GPtrArray *new_calendars);

const gchar *caldav_calendar_get_url(CalDAVCalendar *self);
const gchar *caldav_calendar_get_ctag(CalDAVCalendar *self);
const gchar *caldav_calendar_get_display_name(CalDAVCalendar *self);
const gchar *caldav_calendar_get_color(CalDAVCalendar *self);
const gchar *caldav_calendar_get_sync_token(CalDAVCalendar *self);

void caldav_calendar_set_sync_token(CalDAVCalendar *self, const gchar *sync_token);
void caldav_calendar_set_display_name(CalDAVCalendar *self, const gchar *display_name);
void caldav_calendar_set_color(CalDAVCalendar *self, const gchar *color);
void caldav_calendar_set_ctag(CalDAVCalendar *self, const gchar *ctag);

// ---------- CalDAVTodo ---------- //

#define CALDAV_TYPE_TODO (caldav_todo_get_type())
G_DECLARE_FINAL_TYPE(CalDAVTodo, caldav_todo, CALDAV, TODO, GObject)

CalDAVTodo *caldav_todo_new(icalcomponent *ical, const gchar *url, const gchar *etag);
gboolean caldav_todo_delete(CalDAVClient *c, CalDAVTodo *self);

icalcomponent *caldav_todo_get_ical(CalDAVTodo *self);
const gchar *caldav_todo_get_url(CalDAVTodo *self);
const gchar *caldav_todo_get_etag(CalDAVTodo *self);

gboolean caldav_todo_set_ical(CalDAVTodo *self, CalDAVClient *c, icalcomponent *ical);
void caldav_todo_set_etag(CalDAVTodo *self, const gchar *etag);
void caldav_todo_set_url(CalDAVTodo *self, const gchar *url);

G_END_DECLS
