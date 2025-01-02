#include "sync.h"
#include "lib/caldav.h"
#include "utils.h"
#include "utils/macros.h"

#include <stdio.h>
#include <string.h>

void sync_init() {
  CalDAVClient *client = caldav_client_new("http://127.0.0.1:8080", "vlad", "1710");
  if (!client)
    return;

  // Get calendars
  CalDAVList *calendars = caldav_client_get_calendars(client, "VTODO");
  // for (size_t i = 0; i < caldav_list_length(calendars); i++) {
  //   CalDAVCalendar *calendar = caldav_list_at(calendars, i);
  //   CalDAVEvent *e = caldav_calendar_create_event(
  //       calendar,
  //       "BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VTODO\nUID:1234567891239012391023\nSUMMARY:"
  //                 "Finish the project report\nDESCRIPTION:Complete the final report for the "
  //                 "project and submit it by the due date.\nEND:VTODO\nEND:VCALENDAR");
  //   printf("%s\n%s\n", e->url, e->ical);
  // CalDAVList *events = caldav_calendar_get_events(CALDAV_CALENDAR(caldav_list_at(calendars,
  // i))); for (size_t idx = 0; idx < caldav_list_length(events); idx++) {
  //   CalDAVEvent *event = caldav_list_at(events, idx);
  //   // caldav_event_pull(client, event);
  //   // caldav_event_print(event);
  // }
  // }
}
