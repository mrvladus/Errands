#ifndef CALDAV_H
#define CALDAV_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Client object
typedef struct {
  char *usrpwd; // Username and password in format "user:password"
  char *base_url;
  char *caldav_url;
  char *principal_url;
  char *calendars_url;
} CalDAVClient;

// Calendar object
typedef struct {
  CalDAVClient *client; // Client associated with calendar
  char *name;           // Display name
  char *color;          // Calendar color
  char *set;            // Supported component set like "VTODO" or "VEVENT"
  char *url;            // URL
  char *uuid;           // UUID
  bool deleted;
} CalDAVCalendar;

// Cast to CalDAVCalendar* macro
#define CALDAV_CALENDAR(ptr) (CalDAVCalendar *)ptr

// Event object
typedef struct {
  CalDAVCalendar *calendar;
  char *ical;
  char *url;
  bool deleted;
} CalDAVEvent;

// Cast to CalDAVEvent* macro
#define CALDAV_EVENT(ptr) (CalDAVEvent *)ptr

// Simple dynamic array of pointers that can only grow in size
typedef struct {
  size_t size; // List capacity
  size_t len;  // Number of list elements
  void **data; // Pointer to data array
} CalDAVList;

// Create new list
CalDAVList *caldav_list_new();
// Add element to the list
void caldav_list_add(CalDAVList *list, void *data);
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
// Returns NULL on failure.
CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *client, const char *name, const char *set,
                                              const char *color);
// Free client data
void caldav_client_free(CalDAVClient *client);

// --- EVENT --- //

// Cast to CalDAVEvent* macro
#define CALDAV_EVENT(ptr) (CalDAVEvent *)ptr
// Create new CalDAVEvent
CalDAVEvent *caldav_event_new(CalDAVCalendar *calendar, const char *ical, const char *url);
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

// --- CALENDAR --- //

// Create new calendar object.
// set - "VEVENT", "VTODO", "VJOURNAL".
CalDAVCalendar *caldav_calendar_new(CalDAVClient *client, char *color, char *set, char *name, char *url);
// Delete calendar on server.
// Returns "true" on success and "false" on failure.
bool caldav_calendar_delete(CalDAVCalendar *calendar);
// Fetch events from CalDAVCalendar of type "calendar->set".
// Returns a CalDAVList* with items of type CalDAVEvent*.
CalDAVList *caldav_calendar_get_events(CalDAVCalendar *calendar);
// Create new event on the server.
// Caller is responsible for passing valid ical data:
// - ical must start with BEGIN:VCALENDAR and end with END:VCALENDAR.
// - component set must be the same as calendar->set (VEVENT, VTODO, VJOURNAL). It must start with
//   BEGIN:<component set> and end with END:<component set> and contain at least
//   UID property.
// Returns new event or NULL on failure.
CalDAVEvent *caldav_calendar_create_event(CalDAVCalendar *calendar, const char *ical);
// Print calendar info
void caldav_calendar_print(CalDAVCalendar *calendar);
void caldav_calendar_free(CalDAVCalendar *calendar);

// ---------- ICAL ---------- //

char *caldav_ical_get_prop(const char *ical, const char *prop);
bool caldav_ical_is_valid(const char *ical);

// ---------- UTILS ---------- //

char *caldav_generate_hex_color();

#ifdef __cplusplus
}
#endif

#endif // CALDAV_H
