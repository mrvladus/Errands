#include "caldav-requests.h"
#include "caldav-utils.h"
#include "caldav.h"

#include <libical/ical.h>

CalDAVEvent *caldav_event_new(CalDAVCalendar *calendar, icalcomponent *ical, const char *url) {
  CalDAVEvent *event = malloc(sizeof(CalDAVEvent));
  event->calendar = calendar;
  event->ical = ical;
  event->url = strdup(url);
  event->deleted = false;
  return event;
}

// Delete event on the server.
// Sets event->deleted to "true" on success.
// Returns "true" on success and "false" on failure.
bool caldav_event_delete(CalDAVEvent *event) {
  caldav_log("Delete event at %s", event->url);
  bool res = caldav_delete(event->calendar->client, event->url);
  event->deleted = res;
  return res;
}

// Replace ical data of the event with new, pulled from the server.
// Returns "true" on success and "false" on failure.
bool caldav_event_pull(CalDAVEvent *event) {
  caldav_log("Pull event at %s", event->url);
  char *res = caldav_get(event->calendar->client, event->url);
  if (res) {
    icalcomponent *comp = icalcomponent_new_from_string(res);
    if (comp) {
      icalcomponent_free(event->ical);
      event->ical = comp;
      free(res);
      return true;
    }
    free(res);
  }
  return false;
}

// Update event on the server with event->ical
// Returns "true" on success and "false" on failure.
bool caldav_event_push(CalDAVEvent *event) {
  caldav_log("Push event at %s", event->url);
  return caldav_put(event->calendar->client, event->url, icalcomponent_as_ical_string(event->ical));
}

// Print event info
void caldav_event_print(CalDAVEvent *event) {
  if (event) caldav_log("Event at %s:\n%s", event->url, icalcomponent_as_ical_string(event->ical));
  else caldav_log("Event is NULL");
}

// Cleanup event
void caldav_event_free(CalDAVEvent *event) {
  caldav_free(event->ical);
  caldav_free(event->url);
  icalcomponent_free(event->ical);
  caldav_free(event);
}
