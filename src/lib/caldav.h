#pragma once

#include <stdbool.h>
#include <stddef.h>

// Client object
typedef struct _CalDAVClient CalDAVClient;
// Calendar object
typedef struct _CalDAVCalendar CalDAVCalendar;
// Event object
typedef struct _CalDAVEvent CalDAVEvent;

// --- DYNAMIC ARRAY --- //

// Simple dynamic array of pointers that can only grow in size
typedef struct _CalDAVList CalDAVList;
// Create new list
CalDAVList *caldav_list_new();
// Add element to the list
void caldav_list_add(CalDAVList *list, void *data);
// Get length of the list
size_t caldav_list_length(CalDAVList *list);
// Get element at index. Returns NULL if index out of bounds
void *caldav_list_at(CalDAVList *list, size_t index);
// Cleanup list
void caldav_list_free(CalDAVList *list);

// --- CLIENT --- //

// Initialize caldav client with url, username and password
CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password);
// Fetch calendars from server.
// set - "VEVENT" or "VTODO".
// Returns a CalDAVList* with items of type CalDAVCalendar.
CalDAVList *caldav_client_get_calendars(CalDAVClient *client, const char *set);
// Create new calendar on the server with name, component set ("VEVENT" or "VTODO") and HEX color.
// Returns "true" on success and "false" on failure.
CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *client, const char *name,
                                              const char *set, const char *color);
// Free client data
void caldav_client_free(CalDAVClient *client);

// --- EVENT --- //

// Cast to CalDAVEvent* macro
#define CALDAV_EVENT(ptr) (CalDAVEvent *)ptr
// Create new CalDAVEvent. Free with caldav_event_free().
CalDAVEvent *caldav_event_new(const char *ical, const char *url);
// Set URL of the event
void caldav_event_set_url(CalDAVEvent *event, const char *url);
// Get URL of the event
const char *caldav_event_get_url(CalDAVEvent *event);
// Set iCAL string of the event
void caldav_event_set_ical(CalDAVEvent *event, const char *ical);
// Get iCAL string of the event
const char *caldav_event_get_ical(CalDAVEvent *event);
// Pull event from the server.
// Returns "true" on success and "false" on failure.
bool caldav_event_pull(CalDAVClient *client, CalDAVEvent *event);
// Update / Create event on the server. If event->url not exists - will create new.
// Returns "true" on success and "false" on failure.
bool caldav_event_push(CalDAVClient *client, CalDAVEvent *event);
// Print event info
void caldav_event_print(CalDAVEvent *event);
// Cleanup event
void caldav_event_free(CalDAVEvent *event);

// --- CALENDAR --- //

// Cast to CalDAVCalendar* macro
#define CALDAV_CALENDAR(ptr) (CalDAVCalendar *)ptr
// Create new calendar struct.
// set - "VEVENT" or "VTODO".
CalDAVCalendar *caldav_calendar_new(CalDAVClient *client, char *color, char *set, char *name,
                                    char *url);
const char *caldav_calendar_get_name(CalDAVCalendar *calendar);
// Delete calendar on server
// Returns "true" on success and "false" on failure.
bool caldav_calendar_delete(CalDAVCalendar *calendar);
// Fetch events from CalDAVCalendar of type "calendar->set".
// Returns a CalDAVList* with items of type CalDAVEvent.
CalDAVList *caldav_calendar_get_events(CalDAVCalendar *calendar);
// CalDAVEvent *caldav_calendar_create_event(CalDAVCalendar *calendar, const char *ical);
// Print calendar info
void caldav_calendar_print(CalDAVCalendar *calendar);
void caldav_calendar_free(CalDAVCalendar *calendar);

// ---------- ICAL ---------- //

char *caldav_ical_get_prop(const char *ical, const char *prop);
