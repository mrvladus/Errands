#include "sync.h"
#include "lib/caldav.h"

void sync_init() {
  // CalDAVClient *client = caldav_client_new("http://localhost:8080", "vlad", "1710");
  // if (!client)
  // return;

  // Get calendars
  // CalDAVList *calendars = caldav_client_get_calendars(client, "VTODO");
  // for (size_t i = 0; i < caldav_list_length(calendars); i++) {
  //   CalDAVCalendar *calendar = caldav_list_at(calendars, i);
  //   CalDAVList *events = caldav_calendar_get_events(CALDAV_CALENDAR(caldav_list_at(calendars,
  //   i))); for (size_t idx = 0; idx < caldav_list_length(events); idx++) {
  //     CalDAVEvent *event = caldav_list_at(events, idx);
  //     caldav_event_delete(event);
  //   }
  // }
}
