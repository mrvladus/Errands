#ifndef CALDAV_H
#define CALDAV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Client object
typedef struct {
  char *usrpwd; // Username and password in format "user:password"
  char *base_url;
  char *caldav_url;
  char *principal_url;
  char *calendars_url;
} CalDAVClient;

// Calendar object
typedef struct {
  CalDAVClient *client; // Client associated with calendar
  char *name;           // Display name
  char *color;          // Calendar color
  char *set;            // Supported component set like "VTODO" or "VEVENT"
  char *url;            // URL
  char *uuid;           // UUID
  bool deleted;
} CalDAVCalendar;

// Event object
typedef struct {
  CalDAVCalendar *calendar;
  char *ical;
  char *url;
  bool deleted;
} CalDAVEvent;

// --- DYNAMIC ARRAY --- //

// Simple dynamic array of pointers that can only grow in size
typedef struct {
  size_t size; // List capacity
  size_t len;  // Number of list elements
  void **data; // Pointer to data array
} CalDAVList;

// Create new list
CalDAVList *caldav_list_new();
// Add element to the list
void caldav_list_add(CalDAVList *list, void *data);
// Get element at index. Returns NULL if index out of bounds
void *caldav_list_at(CalDAVList *list, size_t index);
// Cleanup list
void caldav_list_free(CalDAVList *list);

// --- CLIENT --- //

// Initialize caldav client with url, username and password
CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password);
// Fetch calendars from server.
// set - "VEVENT" or "VTODO".
// Returns a CalDAVList* with items of type CalDAVCalendar.
CalDAVList *caldav_client_get_calendars(CalDAVClient *client, const char *set);
// Create new calendar on the server with name, component set ("VEVENT" or "VTODO") and HEX color.
// Returns NULL on failure.
CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *client, const char *name, const char *set,
                                              const char *color);
// Free client data
void caldav_client_free(CalDAVClient *client);

// --- EVENT --- //

// Cast to CalDAVEvent* macro
#define CALDAV_EVENT(ptr) (CalDAVEvent *)ptr
// Create new CalDAVEvent
CalDAVEvent *caldav_event_new(CalDAVCalendar *calendar, const char *ical, const char *url);
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
// Print event info
void caldav_event_print(CalDAVEvent *event);
// Cleanup event
void caldav_event_free(CalDAVEvent *event);

// --- CALENDAR --- //

// Cast to CalDAVCalendar* macro
#define CALDAV_CALENDAR(ptr) (CalDAVCalendar *)ptr
// Create new calendar object.
// set - "VEVENT", "VTODO", "VJOURNAL".
CalDAVCalendar *caldav_calendar_new(CalDAVClient *client, char *color, char *set, char *name, char *url);
// Delete calendar on server.
// Returns "true" on success and "false" on failure.
bool caldav_calendar_delete(CalDAVCalendar *calendar);
// Fetch events from CalDAVCalendar of type "calendar->set".
// Returns a CalDAVList* with items of type CalDAVEvent*.
CalDAVList *caldav_calendar_get_events(CalDAVCalendar *calendar);
// Create new event on the server.
// Caller is responsible for passing valid ical data:
// - ical must start with BEGIN:VCALENDAR and end with END:VCALENDAR.
// - component set must be the same as calendar->set (VEVENT, VTODO, VJOURNAL). It must start with
//   BEGIN:<component set> and end with END:<component set> and contain at least
//   UID property.
// Returns new event or NULL on failure.
CalDAVEvent *caldav_calendar_create_event(CalDAVCalendar *calendar, const char *ical);
// Print calendar info
void caldav_calendar_print(CalDAVCalendar *calendar);
void caldav_calendar_free(CalDAVCalendar *calendar);

// ---------- ICAL ---------- //

char *caldav_ical_get_prop(const char *ical, const char *prop);
bool caldav_ical_is_valid(const char *ical);

// ---------- UTILS ---------- //

char *caldav_generate_hex_color();

#ifdef __cplusplus
}
#endif

#endif // CALDAV_H

// --------------------------------------------------------------------------------------------- //
//                                        IMPLEMENTATION                                         //
// --------------------------------------------------------------------------------------------- //

#ifndef CALDAV_H_IMPLEMENTATION
#define CALDAV_H_IMPLEMENTATION

#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- MACROS --- //

#define CALDAV_LOG(format, ...) printf("[CalDAV] " format "\n", ##__VA_ARGS__)

#define CALDAV_FREE(ptr)                                                                                               \
  if (ptr) {                                                                                                           \
    free(ptr);                                                                                                         \
    ptr = NULL;                                                                                                        \
  }

#define CALDAV_FREE_AND_REPLACE(ptr, data)                                                                             \
  if (ptr) {                                                                                                           \
    free(ptr);                                                                                                         \
    ptr = data;                                                                                                        \
  }

// --- HELPER FUCTIONS --- //

// Extract last part of the URL path
static char *__extract_uuid(const char *path) {
  // Check if the path is empty
  if (!path || *path == '\0') {
    return strdup(""); // Return an empty string if the path is empty
  }
  size_t len = strlen(path);
  size_t end = len - 1;
  if (path[end] == '/')
    end--;
  size_t start = end;
  while (path[start] != '/')
    start--;
  start++;
  char *out = malloc(sizeof(char) * (end - start) + 1);
  strncpy(out, path + start, end - start);
  out[end - start] = '\0';

  return out;
}

static char *__generate_uuid4() {
  srand((unsigned int)time(NULL));
  const char *hex_chars = "0123456789abcdef";
  char *uuid = malloc(37 * sizeof(char)); // Allocate 36 characters + null terminator
  if (!uuid)
    return NULL; // Allocation failed
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static char *strdup_printf(const char *format, ...) {
  va_list args;
  va_list args_copy;
  char *result = NULL;
  int size;

  // Initialize the variable argument list
  va_start(args, format);

  // Copy the argument list to determine required buffer size
  va_copy(args_copy, args);
  size = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);

  if (size < 0) {
    va_end(args);
    return NULL; // Formatting error
  }

  // Allocate memory for the formatted string (size + 1 for null terminator)
  result = malloc(size + 1);
  if (!result) {
    va_end(args);
    return NULL; // Memory allocation failed
  }

  // Format the string into the allocated buffer
  vsnprintf(result, size + 1, format, args);
  va_end(args);

  return result;
}

// ---------- CLIENT ---------- //

struct response_data {
  char *data;
  size_t size;
};

// Callback function to write the response data
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
  struct response_data *__data = (struct response_data *)data;
  size_t total_size = size * nmemb;
  char *temp = realloc(__data->data, __data->size + total_size + 1);
  if (temp == NULL)
    return 0;
  __data->data = temp;
  memcpy(&(__data->data[__data->size]), ptr, total_size);
  __data->size += total_size;
  __data->data[__data->size] = '\0';
  return total_size;
}

// No-op callback to discard response body
static size_t null_write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
  return size * nmemb; // Ignore the data
}

static char *caldav_propfind(CalDAVClient *client, const char *url, size_t depth, const char *body,
                             const char *err_msg) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
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
  curl_easy_setopt(curl, CURLOPT_USERPWD, client->usrpwd);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    CALDAV_LOG("%s: %s", err_msg, curl_easy_strerror(res));
    free(response.data);
    response.data = NULL;
  } else {
  }
  CALDAV_FREE(__depth);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return response.data;
}

static bool caldav_delete(CalDAVClient *client, const char *url, const char *err_msg) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return false;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  curl_easy_setopt(curl, CURLOPT_USERPWD, client->usrpwd);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  bool out = true;
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    CALDAV_LOG("%s: %s", err_msg, curl_easy_strerror(res));
    out = false;
  }
  curl_easy_cleanup(curl);
  return out;
}

static char *caldav_get(CalDAVClient *client, const char *url, const char *err_msg) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return false;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
  curl_easy_setopt(curl, CURLOPT_USERPWD, client->usrpwd);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  struct response_data response = {NULL, 0};
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    CALDAV_LOG("%s: %s", err_msg, curl_easy_strerror(res));
    free(response.data);
  }
  curl_easy_cleanup(curl);
  return response.data;
}

static bool caldav_put(CalDAVClient *client, const char *url, const char *body, const char *err_msg) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: text/calendar");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
  curl_easy_setopt(curl, CURLOPT_USERPWD, client->usrpwd);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  bool out = false;
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK)
    CALDAV_LOG("%s: %s\n", err_msg, curl_easy_strerror(res));
  else
    out = true;
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return out;
}

static char *caldav_client_get_caldav_url(CalDAVClient *client) {
  CALDAV_LOG("Getting CalDAV URL for %s", client->base_url);
  CURL *curl = curl_easy_init();
  if (!curl) {
    CALDAV_LOG("Failed to initialize CURL.");
    return NULL;
  }
  char *out = NULL;
  char discover_url[strlen(client->base_url) + 20];
  sprintf(discover_url, "%s/.well-known/caldav", client->base_url);
  curl_easy_setopt(curl, CURLOPT_URL, discover_url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK)
    CALDAV_LOG("Failed to get CalDAV url: %s", curl_easy_strerror(res));
  else {
    char *effective_url = NULL; // Initialize the pointer
    res = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (res == CURLE_OK && effective_url) {
      out = strdup(effective_url); // Duplicate the effective URL
      if (!out)
        CALDAV_LOG("Failed to allocate memory for the URL.");
      CALDAV_LOG("CalDAV URL: %s", out);
    } else
      CALDAV_LOG("Failed to retrieve effective URL.");
  }
  curl_easy_cleanup(curl); // Cleanup the CURL resource safely
  return out;
}

char *xml_get_tag(const char *xml, const char *tag) {
  xmlDocPtr doc = xmlReadMemory(xml, strlen(xml), NULL, NULL, 0);
  xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
  xmlXPathRegisterNs(xpathCtx, BAD_CAST "d", BAD_CAST "DAV:");
  xmlXPathRegisterNs(xpathCtx, BAD_CAST "cal", BAD_CAST "urn:ietf:params:xml:ns:caldav");
  xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(BAD_CAST tag, xpathCtx);
  xmlNodePtr node = xpathObj->nodesetval->nodeTab[0];
  char *text = (char *)xmlNodeGetContent(node);
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  xmlFreeDoc(doc);
  return text;
}

static char *caldav_client_get_principal_url(CalDAVClient *client) {
  CALDAV_LOG("Getting CalDAV principal URL...");
  char *out = NULL;
  const char *request_body = "<d:propfind xmlns:d=\"DAV:\">"
                             "  <d:prop>"
                             "    <d:current-user-principal />"
                             "  </d:prop>"
                             "</d:propfind>";
  char *xml = caldav_propfind(client, client->caldav_url, 0, request_body, "Failed to get principal URL");
  if (xml) {
    char *href = xml_get_tag(xml, "//d:current-user-principal/d:href");
    out = strdup_printf("%s%s", client->base_url, href);
    CALDAV_LOG("Principal URL: %s", out);
    CALDAV_FREE(href);
    CALDAV_FREE(xml);
  }
  return out;
}

static char *caldav_client_get_calendars_url(CalDAVClient *client) {
  CALDAV_LOG("Getting CalDAV calendars URL...");
  const char *request_body = "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                             "  <d:prop>"
                             "    <c:calendar-home-set />"
                             "  </d:prop>"
                             "</d:propfind>";
  char *out = NULL;
  char *xml = caldav_propfind(client, client->principal_url, 0, request_body, "Failed to get calendars URL");
  if (xml) {
    char *href = xml_get_tag(xml, "//cal:calendar-home-set/d:href");
    out = strdup_printf("%s%s", client->base_url, href);
    CALDAV_LOG("Calendars URL: %s", out);
    CALDAV_FREE(xml);
  }
  return out;
}

CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password) {
  if (!base_url || !username || !password)
    return NULL;
  CALDAV_LOG("Initialize");
  CalDAVClient *client = malloc(sizeof(CalDAVClient));
  client->base_url = strdup(base_url);
  asprintf(&client->usrpwd, "%s:%s", username, password);
  curl_global_init(CURL_GLOBAL_DEFAULT);
  client->caldav_url = caldav_client_get_caldav_url(client);
  if (!client->caldav_url) {
    caldav_client_free(client);
    return NULL;
  }
  client->principal_url = caldav_client_get_principal_url(client);
  if (!client->principal_url) {
    caldav_client_free(client);
    return NULL;
  }
  client->calendars_url = caldav_client_get_calendars_url(client);
  if (!client->calendars_url) {
    caldav_client_free(client);
    return NULL;
  }
  return client;
}

// CalDAVList *caldav_client_get_calendars(CalDAVClient *client, const char *set) {
//   CALDAV_LOG("Getting calendars with '%s' componet set...", set);
//   CURL *curl = curl_easy_init();
//   if (!curl)
//     return NULL;
//   const char *request_body = "<d:propfind xmlns:d=\"DAV:\" xmlns:cs=\"http://calendarserver.org/ns/\" "
//                              "xmlns:c=\"urn:ietf:params:xml:ns:caldav\" xmlns:x1=\"http://apple.com/ns/ical/\">"
//                              "  <d:prop>"
//                              "    <d:resourcetype />"
//                              "    <d:displayname />"
//                              "    <x1:calendar-color />"
//                              "    <cs:getctag />"
//                              "    <c:supported-calendar-component-set />"
//                              "  </d:prop>"
//                              "</d:propfind>";
//   CalDAVList *output = NULL;
//   char *result = caldav_propfind(client, client->calendars_url, 1, request_body, "Failed to get calendars");
//   if (result) {
//     XMLNode *root = xml_parse_string(result);
//     XMLNode *multistatus = xml_node_child_at(root, 0);
//     CalDAVList *calendars_list = caldav_list_new();
//     for (size_t i = 0; i < multistatus->children->len; i++) {
//       XMLNode *response = xml_node_child_at(multistatus, i);
//       XMLNode *supported_set = xml_node_find_tag(response, "supported-calendar-component-set", false);
//       if (supported_set) {
//         char *set_val;
//         for (size_t j = 0; j < supported_set->children->len; j++) {
//           set_val = (char *)xml_node_attr(xml_node_child_at(supported_set, j), "name");
//           if (!strcmp(set_val, set))
//             break;
//         }
//         if (!strcmp(set_val, set)) {
//           XMLNode *deleted = xml_node_find_tag(response, "deleted-calendar", false);
//           if (!deleted) {
//             XMLNode *name = xml_node_find_tag(response, "displayname", false);
//             XMLNode *href = xml_node_find_tag(response, "href", false);
//             XMLNode *color = xml_node_find_tag(response, "calendar-color", false);
//             char *hex_color = color ? strdup(color->text) : caldav_generate_hex_color();
//             char cal_url[strlen(client->base_url) + strlen(href->text) + 1];
//             sprintf(cal_url, "%s%s", client->base_url, href->text);
//             CalDAVCalendar *calendar = caldav_calendar_new(client, hex_color, (char *)set, name->text, cal_url);
//             caldav_list_add(calendars_list, calendar);
//             caldav_calendar_print(calendar);
//             CALDAV_FREE(hex_color);
//           }
//         }
//       }
//     }
//     xml_node_free(root);
//     output = calendars_list;
//   }
//   CALDAV_FREE(result);
//   return output;
// }

CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *client, const char *name, const char *set,
                                              const char *color) {
  CALDAV_LOG("Create calendar '%s'", name);
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
  CalDAVCalendar *output = NULL;
  char *uuid = __generate_uuid4();
  char *url = strdup_printf("%s%s/", client->calendars_url, uuid);
  const char *request_template = "<c:mkcalendar xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\" "
                                 "xmlns:x1=\"http://apple.com/ns/ical/\">"
                                 "  <d:set>"
                                 "    <d:prop>"
                                 "      <d:displayname>%s</d:displayname>"
                                 "      <c:supported-calendar-component-set>"
                                 "        <c:comp name=\"%s\"/>"
                                 "      </c:supported-calendar-component-set>"
                                 "      <x1:calendar-color>%s</x1:calendar-color>"
                                 "    </d:prop>"
                                 "  </d:set>"
                                 "</c:mkcalendar>";
  char *request_body = strdup_printf(request_template, name, set, color);
  CURLcode res;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "MKCALENDAR");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
  curl_easy_setopt(curl, CURLOPT_USERPWD, client->usrpwd);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  res = curl_easy_perform(curl);
  if (res != CURLE_OK)
    CALDAV_LOG("Failed to create calendar: %s", curl_easy_strerror(res));
  else
    output = caldav_calendar_new(client, (char *)color, (char *)set, (char *)name, url);
  curl_slist_free_all(headers);
  CALDAV_FREE(uuid);
  CALDAV_FREE(url);
  CALDAV_FREE(request_body);
  curl_easy_cleanup(curl);
  return output;
}

void caldav_client_free(CalDAVClient *client) {
  CALDAV_LOG("Free CalDAVClient");
  CALDAV_FREE(client->usrpwd);
  CALDAV_FREE(client->base_url);
  CALDAV_FREE(client->caldav_url);
  CALDAV_FREE(client->principal_url);
  CALDAV_FREE(client);
  curl_global_cleanup();
}

// ---------- CALENDAR ---------- //

CalDAVCalendar *caldav_calendar_new(CalDAVClient *client, char *color, char *set, char *name, char *url) {
  CalDAVCalendar *calendar = (CalDAVCalendar *)malloc(sizeof(CalDAVCalendar));
  calendar->client = client;
  calendar->color = strdup(color);
  calendar->set = strdup(set);
  calendar->name = strdup(name);
  calendar->url = strdup(url);
  calendar->uuid = __extract_uuid(url);
  calendar->deleted = false;
  return calendar;
}

bool caldav_calendar_delete(CalDAVCalendar *calendar) {
  char msg[strlen(calendar->url) + 30];
  sprintf(msg, "Failed to delete calendar at %s", calendar->url);
  bool res = caldav_delete(calendar->client, calendar->url, msg);
  calendar->deleted = res;
  return res;
}

// CalDAVList *caldav_calendar_get_events(CalDAVCalendar *calendar) {
//   CURL *curl = curl_easy_init();
//   if (!curl)
//     return NULL;
//   const char *request_template = "<c:calendar-query xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
//                                  "  <d:prop>"
//                                  "    <c:calendar-data/>"
//                                  "  </d:prop>"
//                                  "  <c:filter>"
//                                  "    <c:comp-filter name=\"VCALENDAR\">"
//                                  "      <c:comp-filter name=\"%s\"/>"
//                                  "    </c:comp-filter>"
//                                  "  </c:filter>"
//                                  "</c:calendar-query>";
//   char request_body[strlen(request_template) + 7];
//   sprintf(request_body, request_template, calendar->set);
//   CURLcode res;
//   struct response_data response = {NULL, 0};
//   curl_easy_setopt(curl, CURLOPT_URL, calendar->url);
//   curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "REPORT");
//   struct curl_slist *headers = NULL;
//   headers = curl_slist_append(headers, "Depth: 1");
//   headers = curl_slist_append(headers, "Prefer: return-minimal");
//   headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
//   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
//   curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
//   curl_easy_setopt(curl, CURLOPT_USERPWD, calendar->client->usrpwd);
//   curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
//   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
//   curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
//   res = curl_easy_perform(curl);
//   CalDAVList *output = NULL;
//   if (res != CURLE_OK)
//     CALDAV_LOG("Failed to get events for calendar '%s': %s", calendar->url, curl_easy_strerror(res));
//   else {
//     XMLNode *root = xml_parse_string(response.data);
//     output = caldav_list_new();
//     XMLNode *multistatus = xml_node_child_at(root, 0);
//     for (size_t i = 0; i < multistatus->children->len; i++) {
//       XMLNode *response = xml_node_child_at(multistatus, i);
//       XMLNode *href = xml_node_find_tag(response, "href", false);
//       char url[strlen(calendar->client->base_url) + strlen(href->text) + 1];
//       sprintf(url, "%s%s", calendar->client->base_url, href->text);
//       XMLNode *cal_data = xml_node_find_tag(response, "calendar-data", false);
//       CalDAVEvent *event = caldav_event_new(calendar, cal_data->text, url);
//       caldav_list_add(output, event);
//     }
//     xml_node_free(root);
//   }
//   CALDAV_FREE(response.data);
//   curl_slist_free_all(headers);
//   curl_easy_cleanup(curl);
//   return output;
// }

CalDAVEvent *caldav_calendar_create_event(CalDAVCalendar *calendar, const char *ical) {
  CALDAV_LOG("Create event in calendar %s", calendar->uuid);
  if (!caldav_ical_is_valid(ical)) {
    CALDAV_LOG("ical is invalid");
    return NULL;
  }
  char *uuid = caldav_ical_get_prop(ical, "UID");
  char url[strlen(calendar->url) + strlen(uuid) + 5];
  sprintf(url, "%s%s.ics", calendar->url, uuid);
  char msg[strlen(calendar->url) + 27];
  sprintf(msg, "Failed to create event at %s", calendar->url);
  CalDAVEvent *event = NULL;
  bool success = caldav_put(calendar->client, url, ical, msg);
  if (success) {
    event = caldav_event_new(calendar, ical, url);
    caldav_event_pull(event);
  }
  CALDAV_FREE(uuid);
  return event;
}

void caldav_calendar_print(CalDAVCalendar *calendar) {
  CALDAV_LOG("Calendar '%s' at %s with color '%s'", calendar->name, calendar->url,
             calendar->color ? calendar->color : "");
}

void caldav_calendar_free(CalDAVCalendar *calendar) {
  CALDAV_FREE(calendar->name);
  CALDAV_FREE(calendar->set);
  CALDAV_FREE(calendar->url);
  CALDAV_FREE(calendar->uuid);
  CALDAV_FREE(calendar);
}

// ---------- EVENT ---------- //

CalDAVEvent *caldav_event_new(CalDAVCalendar *calendar, const char *ical, const char *url) {
  CalDAVEvent *event = malloc(sizeof(CalDAVEvent));
  event->calendar = calendar;
  event->ical = strdup(ical);
  event->url = strdup(url);
  event->deleted = false;
  return event;
}

// Delete event on the server.
// Sets event->deleted to "true" on success.
// Returns "true" on success and "false" on failure.
bool caldav_event_delete(CalDAVEvent *event) {
  CALDAV_LOG("Delete event at %s", event->url);
  char msg[strlen(event->url) + 27];
  sprintf(msg, "Failed to delete event at %s", event->url);
  bool res = caldav_delete(event->calendar->client, event->url, msg);
  event->deleted = res;
  return res;
}

// Replace ical data of the event with new, pulled from the server.
// Returns "true" on success and "false" on failure.
bool caldav_event_pull(CalDAVEvent *event) {
  CALDAV_LOG("Pull event at %s", event->url);
  char msg[strlen(event->url) + 25];
  sprintf(msg, "Failed to pull event at %s", event->url);
  char *res = caldav_get(event->calendar->client, event->url, msg);
  if (res) {
    CALDAV_FREE_AND_REPLACE(event->ical, res);
    return true;
  }
  return false;
}

// Update event on the server with event->ical
// Returns "true" on success and "false" on failure.
bool caldav_event_push(CalDAVEvent *event) {
  CALDAV_LOG("Push event at %s", event->url);
  if (!caldav_ical_is_valid(event->ical)) {
    CALDAV_LOG("ical is invalid");
    return false;
  }
  char msg[strlen(event->url) + 25];
  sprintf(msg, "Failed to push event to %s", event->url);
  return caldav_put(event->calendar->client, event->url, event->ical, msg);
}

// Print event info
void caldav_event_print(CalDAVEvent *event) {
  if (event)
    CALDAV_LOG("Event at %s:\n%s\n", event->url, event->ical);
  else
    CALDAV_LOG("Event is NULL");
}

// Cleanup event
void caldav_event_free(CalDAVEvent *event) {
  CALDAV_FREE(event->ical);
  CALDAV_FREE(event->url);
  CALDAV_FREE(event);
}

// ---------- LIST ---------- //

CalDAVList *caldav_list_new() {
  CalDAVList *list = (CalDAVList *)malloc(sizeof(CalDAVList)); // Correct allocation
  if (list == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  list->len = 0;
  list->size = 1;
  list->data = (void **)malloc(list->size * sizeof(void *));
  if (list->data == NULL) {
    fprintf(stderr, "Memory allocation for data array failed\n");
    CALDAV_FREE(list);
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
  if (index >= list->len)
    return NULL;
  return list->data[index];
}

void caldav_list_free(CalDAVList *list) {
  if (list != NULL) {
    CALDAV_FREE(list->data);
    CALDAV_FREE(list);
  }
}

// ---------- ICAL ---------- //

// Get ical prop
char *caldav_ical_get_prop(const char *ical, const char *prop) {
  const char *val_start = strstr(ical, prop);
  if (!val_start)
    return NULL;
  val_start += strlen(prop) + 1;
  const char *val_end = strchr(val_start, '\n');
  if (!val_end)
    return NULL;
  const int val_len = val_end - val_start;
  char *value = malloc(val_len);
  value[val_len] = '\0';
  strncpy(value, val_start, val_len);
  return value;
}

bool caldav_ical_is_valid(const char *ical) {
  bool calendar = strstr(ical, "BEGIN:VCALENDAR") && strstr(ical, "END:VCALENDAR");
  bool event = strstr(ical, "BEGIN:VEVENT") && strstr(ical, "END:VEVENT");
  bool todo = strstr(ical, "BEGIN:VTODO") && strstr(ical, "END:VTODO");
  bool uid = strstr(ical, "UID:");
  return calendar && (event || todo) && uid;
}

// ---------- UTILS ---------- //

char *caldav_generate_hex_color() {
  char *color = (char *)malloc(8 * sizeof(char));
  if (color == NULL)
    return NULL;
  srand(time(NULL));
  int r = rand() % 256;
  int g = rand() % 256;
  int b = rand() % 256;
  sprintf(color, "#%02X%02X%02X", r, g, b);
  return color;
}

#endif // CALDAV_H_IMPLEMENTATION
