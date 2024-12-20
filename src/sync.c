#include "sync.h"
#include "lib/caldav.h"
#include "utils.h"

void sync_init() {
  CalDAVClient *client = caldav_client_new("http://127.0.0.1:8080", "vlad", "1710");
  if (!client)
    return;
  // char color[8];
  // generate_hex(color);
  // caldav_client_create_calendar(client, "Hey", "VTODO", color);
  // Get calendars
  // CalDAVList *calendars = caldav_client_get_calendars(client, "VTODO");
  // CalDAVList *events =
  // caldav_calendar_get_events(client, CALDAV_CALENDAR(caldav_list_at(calendars, i)));
}
