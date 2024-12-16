#include "sync.h"
#include "lib/caldav.h"

void sync_init() {
  CalDAVClient *client = caldav_client_new("http://127.0.0.1:8080", "vlad", "1710");
  if (!client)
    return;
  // Get calendars
  CalDAVList *calendars = caldav_client_get_calendars(client, "VTODO");
  for (size_t i = 0; i < caldav_list_length(calendars); i++) {
    CalDAVList *events =
        caldav_calendar_get_events(client, CALDAV_CALENDAR(caldav_list_at(calendars, i)));
    // for (size_t j; j < events->len; j++)
    //   caldav_event_print(CALDAV_EVENT(events->data[i]));
    // printf("\n");
  }
}
