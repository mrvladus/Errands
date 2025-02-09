#include "sync.h"
#include "caldav.h"

void sync_init() {
  CalDAVClient *client = caldav_client_new("http://localhost:8080", "vlad", "1710");
  if (!client)
    return;

  // Get calendars
  // CalDAVList *calendars = caldav_client_get_calendars(client, "VTODO");
  // for (size_t i = 0; i < calendars->len; i++) {
  //   CalDAVCalendar *calendar = caldav_list_at(calendars, i);
  //   CalDAVList *events = caldav_calendar_get_events(CALDAV_CALENDAR(caldav_list_at(calendars, i)));
  //   for (size_t idx = 0; idx < events->len; idx++) {
  //     CalDAVEvent *event = caldav_list_at(events, idx);
  //     caldav_event_print(event);
  //   }
  // }
}

// char *xml_get_tag(const char *xml, const char *tag) {
//   xmlDocPtr doc = xmlReadMemory(xml, strlen(xml), NULL, NULL, 0);
//   xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
//   xmlXPathRegisterNs(xpathCtx, BAD_CAST "d", BAD_CAST "DAV:");
//   xmlXPathRegisterNs(xpathCtx, BAD_CAST "cal", BAD_CAST "urn:ietf:params:xml:ns:caldav");
//   xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(BAD_CAST xml, xpathCtx);
//   xmlNodePtr node = xpathObj->nodesetval->nodeTab[0];
//   char *text = (char *)xmlNodeGetContent(node);
//   xmlXPathFreeObject(xpathObj);
//   xmlXPathFreeContext(xpathCtx);
//   xmlFreeDoc(doc);
//   return text;
// }
