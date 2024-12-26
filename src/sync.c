#include "sync.h"
#include "lib/caldav.h"
#include <stdio.h>

void sync_init() {
  CalDAVClient *client = caldav_client_new("http://127.0.0.1:8080", "vlad", "1710");
  if (!client)
    return;

  // char color[8];
  // generate_hex(color);
  // caldav_client_create_calendar(client, "Hey", "VTODO", color);
  // Get calendars
  CalDAVList *calendars = caldav_client_get_calendars(client, "VTODO");
  for (size_t i = 0; i < caldav_list_length(calendars); i++) {
    CalDAVList *events = caldav_calendar_get_events(CALDAV_CALENDAR(caldav_list_at(calendars, i)));
    for (size_t idx = 0; idx < caldav_list_length(events); idx++) {
      CalDAVEvent *event = caldav_list_at(events, idx);
      caldav_event_print(event);
    }
  }
}
