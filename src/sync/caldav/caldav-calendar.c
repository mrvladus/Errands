#include "../../vendor/xml.h"
#include "caldav-requests.h"
#include "caldav-utils.h"
#include "caldav.h"
#include <libical/ical.h>

CalDAVCalendar *caldav_calendar_new(CalDAVClient *client, char *color, CalDAVComponentSet set, char *name, char *url) {
  CalDAVCalendar *calendar = malloc(sizeof(CalDAVCalendar));
  calendar->client = client;
  calendar->color = strdup(color);
  calendar->set = set;
  calendar->name = strdup(name);
  calendar->url = strdup(url);
  calendar->uuid = extract_uuid(url);
  calendar->deleted = false;
  calendar->events = caldav_list_new();
  return calendar;
}

bool caldav_calendar_delete(CalDAVCalendar *calendar) {
  calendar->deleted = caldav_delete(calendar->client, calendar->url);
  return calendar->deleted;
}

bool caldav_calendar_pull_events(CalDAVCalendar *calendar) {
  caldav_log("Getting events for calendar '%s'", calendar->url);
  const char *request_template = "<c:calendar-query xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                                 "  <d:prop>"
                                 "    <c:calendar-data/>"
                                 "  </d:prop>"
                                 "  <c:filter>"
                                 "    <c:comp-filter name=\"VCALENDAR\">"
                                 "%s"
                                 "    </c:comp-filter>"
                                 "  </c:filter>"
                                 "</c:calendar-query>";

  char comp_set[105];
  comp_set[0] = '\0';
  if (calendar->set & CALDAV_COMPONENT_SET_VTODO) strcat(comp_set, "<c:comp-filter name=\"VTODO\"/>");
  if (calendar->set & CALDAV_COMPONENT_SET_VEVENT) strcat(comp_set, "<c:comp-filter name=\"VEVENT\"/>");
  if (calendar->set & CALDAV_COMPONENT_SET_VJOURNAL) strcat(comp_set, "<c:comp-filter name=\"VJOURNAL\"/>");

  char request_body[strlen(request_template) + strlen(comp_set) + 1];
  sprintf(request_body, request_template, comp_set);
  char *xml = caldav_report(calendar->client, calendar->url, request_body);
  CalDAVList *events = NULL;
  if (xml) {
    XMLNode *root = xml_parse_string(xml);
    XMLNode *multistatus = xml_node_child_at(root, 0);
    events = caldav_list_new();
    for (size_t i = 1; i < multistatus->children->len; i++) {
      XMLNode *res = xml_node_child_at(multistatus, i);
      XMLNode *href = xml_node_find_tag(res, "href", false);
      char *url = strdup_printf("%s%s", calendar->client->base_url, href->text);
      XMLNode *cal_data = xml_node_find_by_path(res, "propstat/prop/calendar-data", false);
      icalcomponent *ical = icalcomponent_new_from_string(cal_data->text);
      CalDAVEvent *event = caldav_event_new(calendar, ical, url);
      caldav_list_add(events, event);
      free(url);
    }
  } else return false;
  caldav_list_free(calendar->events, (void (*)(void *))caldav_event_free);
  calendar->events = events;
  return true;
}

CalDAVEvent *caldav_calendar_create_event(CalDAVCalendar *calendar, icalcomponent *ical) {
  caldav_log("Create event in calendar %s", calendar->uuid);
  const char *uuid = icalcomponent_get_uid(ical);
  char url[strlen(calendar->url) + strlen(uuid) + 5];
  sprintf(url, "%s%s.ics", calendar->url, uuid);
  CalDAVEvent *event = NULL;
  bool success = caldav_put(calendar->client, url, icalcomponent_as_ical_string(ical));
  if (success) {
    event = caldav_event_new(calendar, ical, url);
    caldav_event_pull(event);
  }
  return event;
}

void caldav_calendar_print(CalDAVCalendar *calendar) {
  caldav_log("Calendar '%s' at %s with color '%s'", calendar->name, calendar->url,
             calendar->color ? calendar->color : "");
}

void caldav_calendar_free(CalDAVCalendar *calendar) {
  caldav_free(calendar->name);
  caldav_free(calendar->url);
  caldav_free(calendar->uuid);
  caldav_list_free(calendar->events, (void (*)(void *))caldav_event_free);
  caldav_free(calendar);
}
