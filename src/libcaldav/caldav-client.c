#include "caldav-requests.h"
#include "caldav-utils.h"
#include "caldav.h"

#define XML_H_IMPLEMENTATION
#include "vendor/xml.h"

#include <curl/curl.h>

// Autodiscover CalDAV server calendars URL
static bool caldav_client_autodiscover(CalDAVClient *client) {
  // Autodiscover CalDAV URL
  caldav_log("Autodiscovering CalDAV URL for %s", client->base_url);
  char *caldav_url = caldav_autodiscover(client);
  if (!caldav_url) return false;
  // Get CalDAV principal URL
  caldav_log("Getting CalDAV principal URL...");
  char *principal_url = NULL;
  const char *principal_request_body = "<d:propfind xmlns:d=\"DAV:\">"
                                       "  <d:prop>"
                                       "    <d:current-user-principal />"
                                       "  </d:prop>"
                                       "</d:propfind>";
  char *principal_xml = caldav_propfind(client, caldav_url, 0, principal_request_body);
  if (principal_xml) {
    XMLNode *root = xml_parse_string(principal_xml);
    XMLNode *href =
        xml_node_find_by_path(root, "multistatus/response/propstat/prop/current-user-principal/href", false);
    principal_url = strdup_printf("%s%s", client->base_url, href->text);
    caldav_log("Principal URL: %s", principal_url);
    free(principal_xml);
    xml_node_free(root);
  } else return false;
  // Get CalDAV calendars URL
  caldav_log("Getting CalDAV calendars URL...");
  const char *calendars_request_body = "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                                       "  <d:prop>"
                                       "    <c:calendar-home-set />"
                                       "  </d:prop>"
                                       "</d:propfind>";
  char *calendars_url = NULL;
  char *calendars_xml = caldav_propfind(client, principal_url, 0, calendars_request_body);
  if (calendars_xml) {
    XMLNode *root = xml_parse_string(calendars_xml);
    XMLNode *href = xml_node_find_by_path(root, "multistatus/response/propstat/prop/calendar-home-set/href", false);
    calendars_url = strdup_printf("%s%s", client->base_url, href->text);
    caldav_log("Calendars URL: %s", calendars_url);
    free(calendars_xml);
    xml_node_free(root);
  } else return false;
  client->calendars_url = calendars_url;
  return true;
}

CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password) {
  if (!base_url || !username || !password) return NULL;
  caldav_log("Creating client");
  CalDAVClient *client = malloc(sizeof(CalDAVClient));
  client->base_url = extract_base_url(base_url);
  client->username = strdup(username);
  client->password = strdup(password);
  client->calendars = caldav_list_new();
  curl_global_init(CURL_GLOBAL_DEFAULT);
  if (!caldav_client_autodiscover(client)) {
    caldav_client_free(client);
    return NULL;
  }
  return client;
}

bool caldav_client_pull_calendars(CalDAVClient *client, CalDAVComponentSet set) {
  caldav_log("Getting calendars from %s", client->calendars_url);
  const char *request_body = "<d:propfind xmlns:d=\"DAV:\" xmlns:cs=\"http://calendarserver.org/ns/\" "
                             "xmlns:c=\"urn:ietf:params:xml:ns:caldav\" xmlns:x1=\"http://apple.com/ns/ical/\">"
                             "  <d:prop>"
                             "    <d:resourcetype />"
                             "    <d:displayname />"
                             "    <x1:calendar-color />"
                             "    <c:supported-calendar-component-set />"
                             "  </d:prop>"
                             "</d:propfind>";
  CalDAVList *calendars = NULL;
  char *xml = caldav_propfind(client, client->calendars_url, 1, request_body);
  if (xml) {
    XMLNode *root = xml_parse_string(xml);
    XMLNode *multistatus = xml_node_child_at(root, 0);
    calendars = caldav_list_new();
    for (size_t i = 1; i < multistatus->children->len; i++) {
      XMLNode *res = xml_node_child_at(multistatus, i);
      XMLNode *is_calendar_type = xml_node_find_by_path(res, "propstat/prop/resourcetype/calendar", false);
      if (!is_calendar_type) continue;
      XMLNode *is_deleted = xml_node_find_by_path(res, "propstat/prop/resourcetype/deleted-calendar", false);
      if (is_deleted) continue;
      XMLNode *supported_set = xml_node_find_tag(res, "supported-calendar-component-set", false);
      bool set_found = false;
      for (size_t si = 0; si < supported_set->children->len; si++) {
        XMLNode *comp = xml_node_child_at(supported_set, si);
        const char *comp_set = xml_node_attr(comp, "name");
        if (set & CALDAV_COMPONENT_SET_VTODO && !strcmp(comp_set, "VTODO")) set_found = true;
        if (set & CALDAV_COMPONENT_SET_VEVENT && !strcmp(comp_set, "VEVENT")) set_found = true;
        if (set & CALDAV_COMPONENT_SET_VJOURNAL && !strcmp(comp_set, "VJOURNAL")) set_found = true;
      }
      if (!set_found) continue;
      XMLNode *href = xml_node_find_tag(res, "href", false);
      char *url = strdup_printf("%s%s", client->base_url, href->text);
      XMLNode *name = xml_node_find_by_path(res, "propstat/prop/displayname", false);
      XMLNode *color = xml_node_find_by_path(res, "propstat/prop/calendar-color", false);
      CalDAVCalendar *new_cal = caldav_calendar_new(client, color ? color->text : "#ffffff", set, name->text, url);
      caldav_log("Found calendar %s at %s", name->text, url);
      caldav_list_add(calendars, new_cal);
      free(url);
    }
    xml_node_free(root);
  } else return false;
  client->calendars = calendars;
  return true;
}

CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *client, const char *name, CalDAVComponentSet set,
                                              const char *color) {
  caldav_log("Create calendar '%s'", name);
  CalDAVCalendar *cal = NULL;
  char *uuid = generate_uuid4();
  char *url = strdup_printf("%s%s/", client->calendars_url, uuid);
  if (!caldav_mkcalendar(client, url, name, color, set)) caldav_log("Failed to create calendar: %s", name);
  else cal = caldav_calendar_new(client, (char *)color, set, (char *)name, url);
  caldav_free(uuid);
  caldav_free(url);
  return cal;
}

void caldav_client_free(CalDAVClient *client) {
  caldav_log("Free CalDAVClient");
  caldav_free(client->username);
  caldav_free(client->password);
  caldav_free(client->base_url);
  caldav_free(client->calendars_url);
  caldav_list_free(client->calendars, (void (*)(void *))caldav_calendar_free);
  caldav_free(client);
  curl_global_cleanup();
}
