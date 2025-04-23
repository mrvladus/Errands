#pragma once

#include <libical/ical.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- FLAGS --- //

typedef enum {
  CALDAV_COMPONENT_SET_VEVENT = 1 << 0,
  CALDAV_COMPONENT_SET_VTODO = 1 << 1,
  CALDAV_COMPONENT_SET_VJOURNAL = 1 << 2
} CalDAVComponentSet;

// --- STRUCTS --- //

// Simple dynamic array of pointers that can only grow in size
typedef struct {
  void **data; // Pointer to data array
  size_t size; // List capacity
  size_t len;  // Number of list elements
} CalDAVList;

// Client object
typedef struct {
  char *username;
  char *password;
  char *base_url;
  char *calendars_url;
  CalDAVList *calendars;
} CalDAVClient;

// Calendar object
typedef struct {
  CalDAVClient *client;   // Client associated with calendar
  char *name;             // Display name
  char *color;            // Calendar color
  char *url;              // URL
  char *uuid;             // UUID
  bool deleted;           // Is deleted
  CalDAVList *events;     // List of events
  CalDAVComponentSet set; // Supported component set
} CalDAVCalendar;

// Event object
typedef struct {
  CalDAVCalendar *calendar;
  icalcomponent *ical;
  char *url;
  bool deleted; // Is deleted
} CalDAVEvent;

// --- FUNCTIONS --- //

// Create new list
CalDAVList *caldav_list_new();
// Add element to the list
void caldav_list_add(CalDAVList *list, void *data);
// Get element at index. Returns NULL if index out of bounds.
void *caldav_list_at(CalDAVList *list, size_t index);
// Cleanup list. data_free_func can be NULL.
void caldav_list_free(CalDAVList *list, void (*data_free_func)(void *));

// --- CLIENT --- //

// Initialize caldav client with url, username and password
CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password);
// Fetch calendars from server.
bool caldav_client_pull_calendars(CalDAVClient *client, CalDAVComponentSet set);
// Create new calendar on the server with name, component set ("VEVENT" or "VTODO") and HEX color.
// Returns NULL on failure.
CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *client, const char *name, CalDAVComponentSet set,
                                              const char *color);
// Cleanup client
void caldav_client_free(CalDAVClient *client);

// --- CALENDAR --- //

// Create new calendar object.
CalDAVCalendar *caldav_calendar_new(CalDAVClient *client, char *color, CalDAVComponentSet set, char *name, char *url);
// Delete calendar on server.
// Returns "true" on success and "false" on failure.
bool caldav_calendar_delete(CalDAVCalendar *calendar);
bool caldav_calendar_pull_events(CalDAVCalendar *calendar);
// Create new event on the server.
// Caller is responsible for passing valid ical data:
// - ical must start with BEGIN:VCALENDAR and end with END:VCALENDAR.
// - component set must be the same as calendar->set (VEVENT, VTODO, VJOURNAL). It must start with
//   BEGIN:<component set> and end with END:<component set> and contain at least
//   UID property.
// Returns new event or NULL on failure.
CalDAVEvent *caldav_calendar_create_event(CalDAVCalendar *calendar, icalcomponent *ical);
// Print calendar info
void caldav_calendar_print(CalDAVCalendar *calendar);
void caldav_calendar_free(CalDAVCalendar *calendar);

// --- EVENT --- //

// Create new CalDAVEvent
CalDAVEvent *caldav_event_new(CalDAVCalendar *calendar, icalcomponent *ical, const char *url);
// Delete event on the server.
// Sets event->deleted to "true" on success.
// Returns "true" on success and "false" on failure.
bool caldav_event_delete(CalDAVEvent *event);
// Replace ical data of the event with new, pulled from the server.
// Returns "true" on success and "false" on failure.
bool caldav_event_pull(CalDAVEvent *event);
// Update event on the server with event->ical
// Returns "true" on success and "false" on failure.
bool caldav_event_push(CalDAVEvent *event);
// Print event info
void caldav_event_print(CalDAVEvent *event);
// Cleanup event
void caldav_event_free(CalDAVEvent *event);

#ifdef __cplusplus
}
#endif
