#ifndef CALDAV_H
#define CALDAV_H

#include <curl/curl.h>
#include <libical/ical.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- ENUMS --- //

typedef enum {
  CALDAV_COMPONENT_SET_VEVENT = 1 << 0,
  CALDAV_COMPONENT_SET_VTODO = 1 << 1,
  CALDAV_COMPONENT_SET_VJOURNAL = 1 << 2
} CalDAVComponentSet;

// --- STRUCTS --- //

// Simple dynamic array of pointers that can only grow in size
typedef struct {
  void **data; // Pointer to data array
  size_t size; // List capacity
  size_t len;  // Number of list elements
} CalDAVList;

// Client object
typedef struct {
  char *username;
  char *password;
  char *base_url;
  char *calendars_url;
  CalDAVList *calendars;
} CalDAVClient;

// Calendar object
typedef struct {
  CalDAVClient *client;   // Client associated with calendar
  char *name;             // Display name
  char *color;            // Calendar color
  char *url;              // URL
  char *uuid;             // UUID
  bool deleted;           // Is deleted
  CalDAVComponentSet set; // Supported component set
  CalDAVList *events;     // List of events
} CalDAVCalendar;

// Event object
typedef struct {
  // Parent calendar.
  CalDAVCalendar *calendar;
  // icalcomponent of type VCALENDAR.
  icalcomponent *ical;
  // URL of event.
  char *url;
  // Is event deleted.
  bool deleted;
} CalDAVEvent;

// --- CLIENT --- //

// Initialize caldav client with url, username and password
CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password);
// Autodiscover calendars URL from base URL.
// Server must support autodiscovery at http://base.url/.well-known/caldav.
bool caldav_client_connect(CalDAVClient *client, bool autodiscover);
// Fetch calendars from server.
// All previous calendars are freed and replaced with new ones.
// This invalidates pointers to old calendars.
bool caldav_client_pull_calendars(CalDAVClient *client, CalDAVComponentSet set);
// Create new calendar on the server with name, component set ("VEVENT" or "VTODO") and HEX color.
// Returns NULL on failure.
CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *client, const char *name, CalDAVComponentSet set,
                                              const char *color);
// Cleanup client
void caldav_client_free(CalDAVClient *client);

// --- CALENDAR --- //

// Create new calendar object.
CalDAVCalendar *caldav_calendar_new(CalDAVClient *client, const char *color, CalDAVComponentSet set, const char *name,
                                    const char *url);
// Free calendar object.
void caldav_calendar_free(CalDAVCalendar *calendar);
// Create new event in calendar on server.
// Adds the event to the calendar->events array on success and returns pointer to the event.
// Returns NULL on failure.
CalDAVEvent *caldav_calendar_create_event(CalDAVCalendar *calendar, icalcomponent *ical);
// Delete calendar on server.
// Returns "true" on success and "false" on failure.
bool caldav_calendar_delete(CalDAVCalendar *calendar);
// Refresh calendar events from server.
// All previous events are freed and replaced with new ones.
// This invalidates pointers to old events.
bool caldav_calendar_pull_events(CalDAVCalendar *calendar);

// --- EVENT --- //

// Create new CalDAVEvent.
// Wraps it in VCALENDAR component if needed and adds UID property if it not exists.
CalDAVEvent *caldav_event_new(CalDAVCalendar *calendar, icalcomponent *ical);
// Cleanup event
void caldav_event_free(CalDAVEvent *event);
// Delete event on the server.
// Sets event->deleted to "true" on success.
// Returns "true" on success and "false" on failure.
bool caldav_event_delete(CalDAVEvent *event);
// Replace ical data of the event with new, pulled from the server.
// Returns "true" on success and "false" on failure.
bool caldav_event_pull(CalDAVEvent *event);
// Update event on the server with event->ical
// Returns "true" on success and "false" on failure.
bool caldav_event_push(CalDAVEvent *event);
// Get the inner event (VEVENT, VTODO, VJOURNAL etc.) from event->ical.
// Because event->ical it's wrapped in VCALENDAR element.
icalcomponent *caldav_event_get_ical_event(CalDAVEvent *event);

// --- LIST --- //

// Create new list
CalDAVList *caldav_list_new();
// Add element to the list
void caldav_list_add(CalDAVList *list, void *data);
// Cleanup list. data_free_func can be NULL.
void caldav_list_free(CalDAVList *list, void (*data_free_func)(void *));

// --- UTILS --- //

// Generate a UUIDv4 string.
// Make a copy of the generated UUID string if you need to keep it between calls.
const char *caldav_generate_uuid4();

// Create VCALENDAR object like this:
//      BEGIN:VCALENDAR
//      VERSION:2.0
//      PRODID:-//caldav.h
//      END:VCALENDAR
icalcomponent *caldav_create_vcalendar();

#ifdef __cplusplus
}
#endif

#endif // CALDAV_H

#ifdef CALDAV_IMPLEMENTATION

#define XML_H_IMPLEMENTATION
#include "xml.h"

// ---------- UTILS ---------- //

static inline void caldav_log(const char *format, ...) {
#ifdef CALDAV_DEBUG
  va_list args;
  printf("[CalDAV] ");
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  printf("\n");
#else
  (void)format;
#endif // CALDAV_DEBUG
}

static inline void caldav_free(void *ptr) {
  if (ptr) {
    free(ptr);
    ptr = NULL;
  }
}

// Function to duplicate a string and format it using printf-style arguments.
// The returned string is dynamically allocated and should be freed by the caller.
static inline char *strdup_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  va_list ap2;
  va_copy(ap2, ap);
  int needed = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (needed < 0) {
    va_end(ap2);
    return NULL;
  }
  char *buf = malloc((size_t)needed + 1);
  if (!buf) {
    va_end(ap2);
    return NULL;
  }
  vsnprintf(buf, (size_t)needed + 1, fmt, ap2);
  va_end(ap2);
  return buf;
}

// Function to extract the base URL from a given URL string.
// The returned string is dynamically allocated and should be freed by the caller.
static inline char *extract_base_url(const char *url) {
  if (!url) return NULL;
  const char *scheme_end = strstr(url, "://"); // Pointer to find the "://"
  size_t base_len = 0;
  if (scheme_end) {
    base_len = (scheme_end - url) + strlen("://");          // Include scheme and "://"
    const char *host_start = scheme_end + strlen("://");    // The start of the host (base URL) part.
    const char *slash_after_host = strchr(host_start, '/'); // Find the next '/' after the host start.
    if (slash_after_host)
      base_len += (slash_after_host - host_start); // Calculate overall length: scheme + "://" + host
    else base_len = strlen(url);                   // No slash after host, use the entire string.
  } else {
    const char *slash = strchr(url, '/'); // No scheme, consider the entire string up to the first slash, if any.
    base_len = slash ? slash - url : strlen(url);
  }
  char *base_url = (char *)malloc(base_len + 1); // Allocate memory for the base URL string (+1 for null terminator)
  if (!base_url) return NULL;
  strncpy(base_url, url, base_len); // Copy the base URL part.
  base_url[base_len] = '\0';        // Null terminate the string
  return base_url;
}

const char *caldav_generate_uuid4() {
  static char uuid[37];
  srand((unsigned int)time(NULL));
  char *hex_chars = "0123456789abcdef";
  for (int i = 0; i < 36; i++) {
    switch (i) {
    case 8:
    case 13:
    case 18:
    case 23: uuid[i] = '-'; break;
    case 14: // UUID version 4
      uuid[i] = '4';
      break;
    case 19:                                 // UUID variant (10xx)
      uuid[i] = hex_chars[(rand() % 4) + 8]; // Random 8-11
      break;
    default: uuid[i] = hex_chars[rand() % 16]; break;
    }
  }
  uuid[36] = '\0'; // Null-terminate the string
  return uuid;
}

// Extract last part of the URL path
static inline char *extract_uuid(const char *url) {
  if (!url || *url == '\0') return strdup(""); // Return an empty string if the path is empty
  size_t len = strlen(url);
  size_t end = len - 1;
  if (url[end] == '/') end--;
  size_t start = end;
  while (url[start] != '/') start--;
  start++;
  char *out = malloc(end - start + 1);
  strncpy(out, url + start, end - start);
  out[end - start] = '\0';
  return out;
}

// Generate a random hexadecimal color code.
static inline char *caldav_generate_hex_color() {
  static char color[10];
  srand(time(NULL));
  int r = rand() % 256;
  int g = rand() % 256;
  int b = rand() % 256;
  sprintf(color, "#%02X%02X%02X", r, g, b);
  return color;
}

icalcomponent *caldav_create_vcalendar() {
  icalcomponent *vcalendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
  icalcomponent_add_property(vcalendar, icalproperty_new_version("2.0"));
  icalcomponent_add_property(vcalendar, icalproperty_new_prodid("-//caldav.h"));
  return vcalendar;
}

// --- REQUESTS --- //

static char *caldav_request_autodiscover(CalDAVClient *client);
static char *caldav_request_propfind(CalDAVClient *client, const char *url, size_t depth, const char *body);
static char *caldav_request_report(CalDAVClient *client, const char *url, const char *body);
static bool caldav_delete(CalDAVClient *client, const char *url);
static char *caldav_get(CalDAVClient *client, const char *url);
static bool caldav_put(CalDAVClient *client, const char *url, const char *body);
static bool caldav_mkcalendar(CalDAVClient *client, const char *url, const char *name, const char *color,
                              CalDAVComponentSet set);

// ---------- LIST ---------- //

CalDAVList *caldav_list_new() {
  CalDAVList *list = (CalDAVList *)malloc(sizeof(CalDAVList));
  if (list == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  list->len = 0;
  list->size = 1;
  list->data = (void **)malloc(list->size * sizeof(void *));
  if (list->data == NULL) {
    fprintf(stderr, "Memory allocation for data array failed\n");
    caldav_free(list);
    return NULL;
  }
  return list;
}

void caldav_list_add(CalDAVList *list, void *data) {
  if (list->len >= list->size) {
    list->size *= 2;
    void **new_data = (void **)realloc(list->data, list->size * sizeof(void *));
    if (new_data == NULL) {
      fprintf(stderr, "Memory reallocation failed\n");
      return;
    }
    list->data = new_data;
  }
  list->data[list->len++] = data;
}

void *caldav_list_at(CalDAVList *list, size_t index) {
  if (index >= list->len) return NULL;
  return list->data[index];
}

void caldav_list_free(CalDAVList *list, void (*data_free_func)(void *)) {
  if (list != NULL) {
    if (data_free_func != NULL)
      for (size_t i = 0; i < list->len; i++) data_free_func(list->data[i]);
    caldav_free(list->data);
    caldav_free(list);
  }
}

// ---------- CLIENT ---------- //

CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password) {
  if (!base_url || !username || !password) return NULL;
  caldav_log("Creating client");
  CalDAVClient *client = malloc(sizeof(CalDAVClient));
  client->base_url = strdup(base_url);
  client->username = strdup(username);
  client->password = strdup(password);
  client->calendars = caldav_list_new();
  curl_global_init(CURL_GLOBAL_DEFAULT);

  return client;
}

bool caldav_client_connect(CalDAVClient *client, bool autodiscover) {
  char *caldav_url = client->base_url;
  char *principal_url = NULL;
  char *calendars_url = NULL;
  XMLNode *root = NULL;
  XMLNode *href = NULL;
  // Autodiscover CalDAV URL
  if (autodiscover) {
    caldav_log("Autodiscovering CalDAV URL for %s", client->base_url);
    caldav_url = caldav_request_autodiscover(client);
    if (!caldav_url) {
      caldav_log("Failed to autodiscover CalDAV URL\n");
      return false;
    }
  }
  // Get CalDAV principal URL
  caldav_log("Getting CalDAV principal URL...");
  const char *principal_request_body = "<d:propfind xmlns:d=\"DAV:\">"
                                       "  <d:prop>"
                                       "    <d:current-user-principal />"
                                       "  </d:prop>"
                                       "</d:propfind>";
  char *principal_xml = caldav_request_propfind(client, caldav_url, 0, principal_request_body);
  if (!principal_xml) {
    caldav_log("Failed PROPFIND request for CalDAV principal URL\n");
    return false;
  }
  root = xml_parse_string(principal_xml);
  href = xml_node_find_by_path(root, "multistatus/response/propstat/prop/current-user-principal/href", false);
  if (!href) {
    caldav_log("Failed to parse XML\n");
    caldav_free(principal_xml);
    xml_node_free(root);
    return false;
  }
  principal_url = strdup_printf("%s%s", client->base_url, href->text);
  caldav_log("Principal URL: %s", principal_url);
  caldav_free(principal_xml);
  xml_node_free(root);
  // Get CalDAV calendars URL
  caldav_log("Getting CalDAV calendars URL...");
  const char *calendars_request_body = "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                                       "  <d:prop>"
                                       "    <c:calendar-home-set />"
                                       "  </d:prop>"
                                       "</d:propfind>";
  char *calendars_xml = caldav_request_propfind(client, principal_url, 0, calendars_request_body);
  if (!calendars_xml) {
    fprintf(stderr, "Failed PROPFIND request for CalDAV calendars URL\n");
    return false;
  }
  root = xml_parse_string(calendars_xml);
  href = xml_node_find_by_path(root, "multistatus/response/propstat/prop/calendar-home-set/href", false);
  if (!href) {
    caldav_log("Failed to parse XML\n");
    caldav_free(calendars_xml);
    xml_node_free(root);
    return false;
  }
  calendars_url = strdup_printf("%s%s", client->base_url, href->text);
  caldav_log("Calendars URL: %s", calendars_url);
  caldav_free(calendars_xml);
  xml_node_free(root);
  client->calendars_url = calendars_url;

  return true;
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
  char *xml = caldav_request_propfind(client, client->calendars_url, 1, request_body);
  if (!xml) {
    caldav_log("Failed PROPFIND request to %s", client->calendars_url);
    return false;
  }
  XMLNode *root = xml_parse_string(xml);
  XMLNode *multistatus = xml_node_child_at(root, 0);
  calendars = caldav_list_new();
  for (size_t i = 1; i < multistatus->children->len; i++) {
    XMLNode *res = xml_node_child_at(multistatus, i);
    if (!res) continue;
    XMLNode *is_calendar_type = xml_node_find_by_path(res, "propstat/prop/resourcetype/calendar", false);
    if (!is_calendar_type) continue;
    XMLNode *is_deleted = xml_node_find_by_path(res, "propstat/prop/resourcetype/deleted-calendar", false);
    if (is_deleted) continue;
    XMLNode *supported_set = xml_node_find_tag(res, "supported-calendar-component-set", false);
    if (!supported_set) continue;
    bool set_found = false;
    for (size_t si = 0; si < supported_set->children->len; si++) {
      XMLNode *comp = xml_node_child_at(supported_set, si);
      if (!comp) continue;
      const char *comp_set = xml_node_attr(comp, "name");
      if (!comp_set) continue;
      if (set & CALDAV_COMPONENT_SET_VTODO && !strcmp(comp_set, "VTODO")) set_found = true;
      if (set & CALDAV_COMPONENT_SET_VEVENT && !strcmp(comp_set, "VEVENT")) set_found = true;
      if (set & CALDAV_COMPONENT_SET_VJOURNAL && !strcmp(comp_set, "VJOURNAL")) set_found = true;
    }
    if (!set_found) continue;
    XMLNode *href = xml_node_find_tag(res, "href", false);
    if (!href || !href->text) continue;
    XMLNode *name = xml_node_find_by_path(res, "propstat/prop/displayname", false);
    if (!name || !name->text) continue;
    XMLNode *color = xml_node_find_by_path(res, "propstat/prop/calendar-color", false);
    if (!color || !color->text) continue;
    char *url = strdup_printf("%s%s", client->base_url, href->text);
    CalDAVCalendar *new_cal = caldav_calendar_new(client, color ? color->text : "#ffffff", set, name->text, url);
    caldav_list_add(calendars, new_cal);
    caldav_log("Found calendar at %s", url);
    caldav_free(url);
  }
  xml_node_free(root);
  client->calendars = calendars;
  return true;
}

CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *client, const char *name, CalDAVComponentSet set,
                                              const char *color) {
  CalDAVCalendar *cal = NULL;
  char *url = strdup_printf("%s%s/", client->calendars_url, caldav_generate_uuid4());
  if (!caldav_mkcalendar(client, url, name, color, set)) caldav_log("Failed to create calendar: %s", name);
  else {
    cal = caldav_calendar_new(client, (char *)color, set, (char *)name, url);
    caldav_list_add(client->calendars, cal);
    caldav_log("Created calendar '%s' at %s", name, url);
  }
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

// ---------- CALENDAR ---------- //

CalDAVCalendar *caldav_calendar_new(CalDAVClient *client, const char *color, CalDAVComponentSet set, const char *name,
                                    const char *url) {
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

CalDAVEvent *caldav_calendar_create_event(CalDAVCalendar *calendar, icalcomponent *ical) {
  CalDAVEvent *new_event = caldav_event_new(calendar, ical);
  bool res = caldav_event_push(new_event);
  if (!res) {
    caldav_event_free(new_event);
    return NULL;
  }
  caldav_list_add(calendar->events, new_event);
  return new_event;
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
  CalDAVList *events = NULL;
  char *xml = caldav_request_report(calendar->client, calendar->url, request_body);
  if (!xml) return false;
  XMLNode *root = xml_parse_string(xml);
  if (!root) return false;
  XMLNode *multistatus = xml_node_child_at(root, 0);
  if (!multistatus) return false;
  events = caldav_list_new();
  for (size_t i = 0; i < multistatus->children->len; i++) {
    XMLNode *res = xml_node_child_at(multistatus, i);
    if (!res) continue;
    XMLNode *href = xml_node_find_tag(res, "href", false);
    if (!href || !href->text) continue;
    XMLNode *cal_data = xml_node_find_by_path(res, "propstat/prop/calendar-data", false);
    if (!cal_data || !cal_data->text) continue;
    char *url = strdup_printf("%s%s", calendar->client->base_url, href->text);
    icalcomponent *ical = icalcomponent_new_from_string(cal_data->text);
    CalDAVEvent *event = caldav_event_new(calendar, ical);
    caldav_list_add(events, event);
    caldav_log("Found event at %s", url);
    caldav_free(url);
  }
  caldav_list_free(calendar->events, (void (*)(void *))caldav_event_free);
  calendar->events = events;
  return true;
}

void caldav_calendar_free(CalDAVCalendar *calendar) {
  caldav_log("Free CalDAVCalendar '%s'", calendar->name);
  caldav_free(calendar->name);
  caldav_free(calendar->url);
  caldav_free(calendar->uuid);
  caldav_list_free(calendar->events, (void (*)(void *))caldav_event_free);
  caldav_free(calendar);
}

// ---------- EVENT ---------- //

CalDAVEvent *caldav_event_new(CalDAVCalendar *calendar, icalcomponent *ical) {
  if (!calendar || !ical || !icalcomponent_is_valid(ical)) return NULL;
  CalDAVEvent *event = malloc(sizeof(CalDAVEvent));
  event->calendar = calendar;
  const char *uid = NULL;
  if (icalcomponent_isa(ical) != ICAL_VCALENDAR_COMPONENT) {
    icalcomponent *wrapper = caldav_create_vcalendar();
    icalcomponent_add_component(wrapper, event->ical);
    uid = icalcomponent_get_uid(ical);
    if (!uid) {
      uid = caldav_generate_uuid4();
      icalcomponent_set_uid(ical, uid);
    }
    event->ical = wrapper;
  } else {
    icalcomponent *inner = icalcomponent_get_inner(ical);
    if (inner) uid = icalcomponent_get_uid(inner);
    if (!uid) {
      uid = caldav_generate_uuid4();
      icalcomponent_set_uid(inner, uid);
    }
    event->ical = ical;
  }
  event->url = strdup_printf("%s%s.ics", calendar->url, uid);
  event->deleted = false;
  return event;
}

bool caldav_event_delete(CalDAVEvent *event) {
  if (!event) return false;
  caldav_log("Delete event at %s", event->url);
  bool res = caldav_delete(event->calendar->client, event->url);
  event->deleted = res;
  return res;
}

bool caldav_event_pull(CalDAVEvent *event) {
  if (!event) false;
  caldav_log("Pull event at %s", event->url);
  char *res = caldav_get(event->calendar->client, event->url);
  if (!res) return false;
  icalcomponent *comp = icalcomponent_new_from_string(res);
  if (!comp) return false;
  icalcomponent_free(event->ical);
  event->ical = comp;
  caldav_free(res);
  return true;
}

bool caldav_event_push(CalDAVEvent *event) {
  if (!event || !event->ical || !icalcomponent_is_valid(event->ical)) return false;
  caldav_log("Push event at %s", event->url);
  return caldav_put(event->calendar->client, event->url, icalcomponent_as_ical_string(event->ical));
}

void caldav_event_free(CalDAVEvent *event) {
  if (!event) return;
  caldav_log("Free CalDAVEvent '%s'", event->url);
  caldav_free(event->url);
  icalcomponent_free(event->ical);
  caldav_free(event);
}

icalcomponent *caldav_event_get_ical_event(CalDAVEvent *event) { return icalcomponent_get_inner(event->ical); }

// ---------- REQUESTS ---------- //

struct response_data {
  char *data;
  size_t size;
};

// Callback function to write the response data
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
  struct response_data *_data = (struct response_data *)data;
  size_t total_size = size * nmemb;
  char *temp = realloc(_data->data, _data->size + total_size + 1);
  if (temp == NULL) return 0;
  _data->data = temp;
  memcpy(&(_data->data[_data->size]), ptr, total_size);
  _data->size += total_size;
  _data->data[_data->size] = '\0';
  return total_size;
}

// No-op callback to discard response body
static size_t null_write_callback(void *ptr, size_t size, size_t nmemb, void *data) { return size * nmemb; }

static char *caldav_request_autodiscover(CalDAVClient *client) {
  CURL *curl = curl_easy_init();
  if (!curl) return NULL;
  char *caldav_url = NULL;
  char discover_url[strlen(client->base_url) + 20];
  sprintf(discover_url, "%s/.well-known/caldav", client->base_url);
  curl_easy_setopt(curl, CURLOPT_URL, discover_url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  caldav_log("Request: %s", discover_url);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    caldav_log("Failed to get CalDAV url: %s", curl_easy_strerror(res));
    return NULL;
  } else {
    char *effective_url = NULL;
    res = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (res == CURLE_OK && effective_url) {
      caldav_url = strdup(effective_url);
      caldav_log("CalDAV URL: %s", caldav_url);
    } else {
      caldav_log("Failed to retrieve effective URL.");
      return NULL;
    }
  }
  curl_easy_cleanup(curl);
  return caldav_url;
}

static char *caldav_request_propfind(CalDAVClient *client, const char *url, size_t depth, const char *body) {
  CURL *curl = curl_easy_init();
  if (!curl) return NULL;
  struct response_data response = {NULL, 0};
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
  struct curl_slist *headers = NULL;
  char *__depth = strdup_printf("Depth: %d", (int)depth);
  headers = curl_slist_append(headers, __depth);
  headers = curl_slist_append(headers, "Prefer: return-minimal");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
  curl_easy_setopt(curl, CURLOPT_USERNAME, client->username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, client->password);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    caldav_log("%s", curl_easy_strerror(res));
    caldav_free(response.data);
    response.data = NULL;
  }
  caldav_free(__depth);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return response.data;
}

static bool caldav_delete(CalDAVClient *client, const char *url) {
  CURL *curl = curl_easy_init();
  if (!curl) return false;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  curl_easy_setopt(curl, CURLOPT_USERNAME, client->username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, client->password);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  bool out = true;
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    caldav_log("%s", curl_easy_strerror(res));
    out = false;
  }
  curl_easy_cleanup(curl);
  return out;
}

static char *caldav_get(CalDAVClient *client, const char *url) {
  CURL *curl = curl_easy_init();
  if (!curl) return NULL;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
  curl_easy_setopt(curl, CURLOPT_USERNAME, client->username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, client->password);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  struct response_data response = {NULL, 0};
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    caldav_log("%s", curl_easy_strerror(res));
    caldav_free(response.data);
  }
  curl_easy_cleanup(curl);
  return response.data;
}

static bool caldav_put(CalDAVClient *client, const char *url, const char *body) {
  bool out = false;
  CURL *curl = curl_easy_init();
  if (!curl) return out;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: text/calendar");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
  curl_easy_setopt(curl, CURLOPT_USERNAME, client->username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, client->password);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) caldav_log("%s", curl_easy_strerror(res));
  else out = true;
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return out;
}

static char *caldav_request_report(CalDAVClient *client, const char *url, const char *body) {
  CURL *curl = curl_easy_init();
  if (!curl) return NULL;
  struct response_data response = {NULL, 0};
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "REPORT");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 1");
  headers = curl_slist_append(headers, "Prefer: return-minimal");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
  curl_easy_setopt(curl, CURLOPT_USERNAME, client->username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, client->password);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) caldav_log("%s", curl_easy_strerror(res));
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return response.data;
}

static bool caldav_mkcalendar(CalDAVClient *client, const char *url, const char *name, const char *color,
                              CalDAVComponentSet set) {
  CURL *curl = curl_easy_init();
  if (!curl) return false;
  const char *request_template = "<c:mkcalendar xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\" "
                                 "xmlns:x1=\"http://apple.com/ns/ical/\">"
                                 "  <d:set>"
                                 "    <d:prop>"
                                 "      <d:displayname>%s</d:displayname>"
                                 "      <c:supported-calendar-component-set>"
                                 "%s"
                                 "      </c:supported-calendar-component-set>"
                                 "      <x1:calendar-color>%s</x1:calendar-color>"
                                 "    </d:prop>"
                                 "  </d:set>"
                                 "</c:mkcalendar>";

  char comp_set[60];
  comp_set[0] = '\0';
  if (set & CALDAV_COMPONENT_SET_VTODO) strcat(comp_set, "<c:comp name=\"VTODO\"/>");
  if (set & CALDAV_COMPONENT_SET_VEVENT) strcat(comp_set, "<c:comp name=\"VEVENT\"/>");
  if (set & CALDAV_COMPONENT_SET_VJOURNAL) strcat(comp_set, "<c:comp name=\"VJOURNAL\"/>");

  char *request_body = strdup_printf(request_template, name, comp_set, color);
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "MKCALENDAR");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
  curl_easy_setopt(curl, CURLOPT_USERNAME, client->username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, client->password);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(headers);
  caldav_free(request_body);
  curl_easy_cleanup(curl);
  return res == CURLE_OK;
}

#endif // CALDAV_IMPLEMENTATION
