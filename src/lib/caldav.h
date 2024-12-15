#pragma once

#include <stddef.h>

// --- CLIENT --- //

typedef struct {
  char *usrpwd;
  char *base_url;
  char *caldav_url;
  char *principal_url;
  char *calendars_url;
} CalDAVClient;

// Initialize caldav client with url, username and password
CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password);
// Free client data
void caldav_client_free(CalDAVClient *client);

// --- DYNAMIC ARRAY --- //

typedef struct {
  size_t size;
  size_t len;
  void **data;
} CalDAVList;

CalDAVList *caldav_list_new();
void caldav_list_add(CalDAVList *list, void *data);
void caldav_list_free(CalDAVList *list);

// --- CALENDARS --- //

// CalDAV calendar
typedef struct {
  // Display name
  char *name;
  // Supported component set like "VTODO" or "VEVENT"
  char *set;
  // URL
  char *url;
  // UUID
  char *uuid;
} CalDAVCalendar;

// Cast to CalDAVCalendar* macro
#define CALDAV_CALENDAR(ptr) (CalDAVCalendar *)ptr

// Create new calendar struct.
// set - "VEVENT" or "VTODO".
CalDAVCalendar *caldav_calendar_new(char *set, char *name, char *url);
// Fetch calendars from server.
// set - "VEVENT" or "VTODO".
// Returns a CalDAVList* with items of type CalDAVCalendar.
CalDAVList *caldav_get_calendars(CalDAVClient *client, const char *set);
// Fetch events from CalDAVCalendar of type "calendar->set".
// Returns a CalDAVList* with items of type CalDAVEvent.
CalDAVList *caldav_calendar_get_events(CalDAVClient *client, CalDAVCalendar *calendar);
// Print calendar info
void caldav_calendar_print(CalDAVCalendar *calendar);
void caldav_calendar_free(CalDAVCalendar *calendar);

// --- EVENTS --- //

// CalDAV event. Contains ical string and url of the event.
typedef struct {
  char *ical;
  char *url;
} CalDAVEvent;

// Cast to CalDAVEvent* macro
#define CALDAV_EVENT(ptr) (CalDAVEvent *)ptr

CalDAVEvent *caldav_event_new(char *ical, char *url);
// Print event info
void caldav_event_print(CalDAVEvent *event);
void caldav_event_free(CalDAVEvent *event);

// ---------- ICAL ---------- //

// char *caldav_ical_get_prop(const char *ical);
