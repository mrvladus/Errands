#include "sync.h"
#include "caldav.h"
#include "settings.h"
#include "utils.h"

void sync_init() {
  LOG("Sync: Initialize");
  if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b) {
    LOG("Sync: Sync is disabled");
    return;
  }
  CalDAVClient *client = caldav_client_new("http://localhost:8080", "vlad", "1710");
  if (!client)
    return;

  // Get calendars
  CalDAVList *calendars = caldav_client_get_calendars(client, "VTODO");
  for (size_t i = 0; i < calendars->len; i++) {
    CalDAVCalendar *calendar = caldav_list_at(calendars, i);
    caldav_calendar_print(calendar);
    CalDAVList *events = caldav_calendar_get_events(CALDAV_CALENDAR(caldav_list_at(calendars, i)));
    // for (size_t idx = 0; idx < events->len; idx++) {
    //   CalDAVEvent *event = caldav_list_at(events, idx);
    //   caldav_event_print(event);
    // }
  }
}

void sync_list(ListData *list) {
  if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b) {
    LOG("Sync: Sync is disabled");
    return;
  }
}
void sync_task(TaskData *task) {
  if (!errands_settings_get("sync", SETTING_TYPE_BOOL).b) {
    LOG("Sync: Sync is disabled");
    return;
  }
}
