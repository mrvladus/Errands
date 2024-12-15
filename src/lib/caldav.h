#pragma once

#include <stddef.h>

// Initialize caldav client with url, username and password
void caldav_init(const char *base_url, const char *username, const char *password);
// Cleanup all the memory allocations
void caldav_cleanup();

typedef struct CalDAVList {
  size_t size;
  size_t len;
  void **data;
} CalDAVList;

CalDAVList *caldav_list_new();
void caldav_list_add(CalDAVList *list, void *data);
void caldav_list_free(CalDAVList *list);

// --- CALENDARS --- //

typedef struct CalDAVCalendar {
  char *name;
  char *set;
  char *url;
  char *uuid;
} CalDAVCalendar;

// Cast to CalDAVCalendar* macro
#define CALDAV_CALENDAR(ptr) (CalDAVCalendar *)ptr

// Create new calendar struct.
// set - "VEVENT" or "VTODO"
CalDAVCalendar *caldav_calendar_new(char *set, char *name, char *url);
// Fetch calendars from server
CalDAVList *caldav_get_calendars(const char *set);
// Fetch events from CalDAVCalendar of type "calendar->set".
// Returns a CalDAVList* with items of type CalDAVEvent.
CalDAVList *caldav_calendar_get_events(CalDAVCalendar *calendar);
// Print calendar info
void caldav_calendar_print(CalDAVCalendar *calendar);

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
