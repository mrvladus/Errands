#include "caldav.h"
// #define XML_H_DEBUG
#include "xml.h"

#include <curl/curl.h>

#include <curl/easy.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALDAV_LOG(format, ...) printf("[CalDAV] " format "\n", ##__VA_ARGS__)
#define CALDAV_FREE(ptr)                                                                           \
  if (ptr) {                                                                                       \
    free(ptr);                                                                                     \
    ptr = NULL;                                                                                    \
  }

// --- STRUCTS --- //

struct _CalDAVClient {
  char *usrpwd;
  char *base_url;
  char *caldav_url;
  char *principal_url;
  char *calendars_url;
};

struct _CalDAVCalendar {
  CalDAVClient *client; // Client associated with calendar
  char *name;           // Display name
  char *color;          // Calendar color
  char *set;            // Supported component set like "VTODO" or "VEVENT"
  char *url;            // URL
  char *uuid;           // UUID
};

struct _CalDAVEvent {
  char *ical;
  char *url;
};

struct _CalDAVList {
  size_t size;
  size_t len;
  void **data;
};

// --- HELPER FUCTIONS --- //

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

static char *caldav_propfind(const char *url, const char *usrpwd, size_t depth, const char *body,
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
  curl_easy_setopt(curl, CURLOPT_USERPWD, usrpwd);
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

static char *caldav_client_get_caldav_url(CalDAVClient *client) {
  CALDAV_LOG("Getting CalDAV URL...");
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
  char *out = NULL;
  char discover_url[strlen(client->base_url) + 20];
  sprintf(discover_url, "%s/.well-known/caldav", client->base_url);
  curl_easy_setopt(curl, CURLOPT_URL, discover_url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    CALDAV_LOG("Failed to get CalDAV url: %s", curl_easy_strerror(res));
  } else {
    char *effective_url;
    res = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    out = strdup(effective_url);
    CALDAV_FREE(effective_url);
    CALDAV_LOG("%s", out);
  }
  curl_easy_cleanup(curl);
  return out;
}

static char *caldav_client_get_principal_url(CalDAVClient *client) {
  CALDAV_LOG("Getting CalDAV principal URL...");
  char *out = NULL;
  const char *request_body = "<d:propfind xmlns:d=\"DAV:\">"
                             "  <d:prop>"
                             "    <d:current-user-principal />"
                             "  </d:prop>"
                             "</d:propfind>";
  char *result = caldav_propfind(client->caldav_url, client->usrpwd, 0, request_body,
                                 "Failed to get principal URL");
  if (result) {
    XMLNode *root = xml_parse_string(result);
    XMLNode *href = xml_node_find_by_path(
        root, "multistatus/response/propstat/prop/current-user-principal/href", false);
    char url[strlen(href->text) + strlen(client->base_url) + 1];
    sprintf(url, "%s%s", client->base_url, href->text);
    out = strdup(url);
    xml_node_free(root);
    CALDAV_LOG("%s", out);
  }
  CALDAV_FREE(result);
  return out;
}

static char *caldav_client_get_calendars_url(CalDAVClient *client) {
  CALDAV_LOG("Getting CalDAV calendars URL...");
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
  char *out = NULL;
  const char *request_body =
      "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
      "  <d:prop>"
      "    <c:calendar-home-set />"
      "  </d:prop>"
      "</d:propfind>";
  curl_easy_setopt(curl, CURLOPT_URL, client->principal_url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 0");
  headers = curl_slist_append(headers, "Prefer: return-minimal");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
  curl_easy_setopt(curl, CURLOPT_USERPWD, client->usrpwd);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  struct response_data response = {NULL, 0};
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    CALDAV_LOG("Failed to get calendars URL: %s", curl_easy_strerror(res));
  } else {
    XMLNode *root = xml_parse_string(response.data);
    XMLNode *href = xml_node_find_by_path(
        root, "multistatus/response/propstat/prop/calendar-home-set/href", false);
    char url[strlen(href->text) + strlen(client->base_url) + 1];
    sprintf(url, "%s%s", client->base_url, href->text);
    out = strdup(url);
    xml_node_free(root);
    CALDAV_LOG("%s", url);
  }
  CALDAV_FREE(response.data);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return out;
}

CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password) {
  if (!base_url || !username || !password)
    return NULL;
  CALDAV_LOG("Initialize");
  CalDAVClient *client = malloc(sizeof(CalDAVClient));
  client->base_url = strdup(base_url);
  client->usrpwd = strdup_printf("%s:%s", username, password);
  client->caldav_url = caldav_client_get_caldav_url(client);
  if (!client->caldav_url) {
    caldav_client_free(client);
    return NULL;
  }
  client->principal_url = caldav_client_get_principal_url(client);
  if (!client->caldav_url) {
    caldav_client_free(client);
    return NULL;
  }
  client->calendars_url = caldav_client_get_calendars_url(client);
  if (!client->caldav_url) {
    caldav_client_free(client);
    return NULL;
  }
  return client;
}

CalDAVList *caldav_client_get_calendars(CalDAVClient *client, const char *set) {
  CALDAV_LOG("Getting calendars with '%s' componet set...", set);
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
  const char *request_body =
      "<d:propfind xmlns:d=\"DAV:\" xmlns:cs=\"http://calendarserver.org/ns/\" "
      "xmlns:c=\"urn:ietf:params:xml:ns:caldav\" xmlns:x1=\"http://apple.com/ns/ical/\">"
      "  <d:prop>"
      "    <d:resourcetype />"
      "    <d:displayname />"
      "    <x1:calendar-color />"
      "    <cs:getctag />"
      "    <c:supported-calendar-component-set />"
      "  </d:prop>"
      "</d:propfind>";
  CURLcode res;
  struct response_data response = {NULL, 0};
  curl_easy_setopt(curl, CURLOPT_URL, client->calendars_url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 1");
  headers = curl_slist_append(headers, "Prefer: return-minimal");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
  curl_easy_setopt(curl, CURLOPT_USERPWD, client->usrpwd);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  res = curl_easy_perform(curl);
  CalDAVList *output = NULL;
  if (res != CURLE_OK) {
    CALDAV_LOG("Failed to get calendars: %s", curl_easy_strerror(res));
  } else {
    XMLNode *root = xml_parse_string(response.data);
    XMLNode *multistatus = xml_node_child_at(root, 0);
    CalDAVList *calendars_list = caldav_list_new();
    for (size_t i = 0; i < multistatus->children->len; i++) {
      XMLNode *response = xml_node_child_at(multistatus, i);
      XMLNode *supported_set =
          xml_node_find_tag(response, "supported-calendar-component-set", false);
      if (supported_set) {
        const char *set_val = xml_node_attr(xml_node_child_at(supported_set, 0), "name");
        if (!strcmp(set, set) && !strcmp(set_val, set)) {
          XMLNode *deleted = xml_node_find_tag(response, "deleted-calendar", false);
          if (!deleted) {
            XMLNode *name = xml_node_find_tag(response, "displayname", false);
            XMLNode *color = xml_node_find_tag(response, "calendar-color", false);
            XMLNode *href = xml_node_find_tag(response, "href", false);
            char *cal_url = strdup_printf("%s%s", client->base_url, href->text);
            CalDAVCalendar *calendar =
                caldav_calendar_new(client, color->text, (char *)set, name->text, cal_url);
            caldav_list_add(calendars_list, calendar);
            caldav_calendar_print(calendar);
            CALDAV_FREE(cal_url);
          }
        }
      }
    }
    xml_node_free(root);
    output = calendars_list;
  }
  CALDAV_FREE(response.data);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return output;
}

CalDAVCalendar *caldav_client_create_calendar(CalDAVClient *client, const char *name,
                                              const char *set, const char *color) {
  CALDAV_LOG("Create calendar '%s'", name);
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
  CalDAVCalendar *output = NULL;
  char *uuid = __generate_uuid4();
  char *url = strdup_printf("%s%s/", client->calendars_url, uuid);
  const char *request_template =
      "<c:mkcalendar xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\" "
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

CalDAVCalendar *caldav_calendar_new(CalDAVClient *client, char *color, char *set, char *name,
                                    char *url) {
  CalDAVCalendar *calendar = (CalDAVCalendar *)malloc(sizeof(CalDAVCalendar));
  calendar->client = client;
  calendar->color = color;
  calendar->set = set;
  calendar->name = strdup(name);
  calendar->url = strdup(url);
  calendar->uuid = __extract_uuid(url);
  return calendar;
}

CalDAVList *caldav_calendar_get_events(CalDAVCalendar *calendar) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
  const char *request_template =
      "<c:calendar-query xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
      "  <d:prop>"
      "    <c:calendar-data/>"
      "  </d:prop>"
      "  <c:filter>"
      "    <c:comp-filter name=\"VCALENDAR\">"
      "      <c:comp-filter name=\"%s\"/>"
      "    </c:comp-filter>"
      "  </c:filter>"
      "</c:calendar-query>";
  char request_body[strlen(request_template) + 7];
  sprintf(request_body, request_template, calendar->set);
  CURLcode res;
  struct response_data response = {NULL, 0};
  curl_easy_setopt(curl, CURLOPT_URL, calendar->url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "REPORT");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 1");
  headers = curl_slist_append(headers, "Prefer: return-minimal");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
  curl_easy_setopt(curl, CURLOPT_USERPWD, calendar->client->usrpwd);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  res = curl_easy_perform(curl);
  CalDAVList *output = NULL;
  if (res != CURLE_OK)
    CALDAV_LOG("Failed to get events for calendar '%s': %s", calendar->url,
               curl_easy_strerror(res));
  else {
    XMLNode *root = xml_parse_string(response.data);
    output = caldav_list_new();
    XMLNode *multistatus = xml_node_child_at(root, 0);
    for (size_t i = 0; i < multistatus->children->len; i++) {
      XMLNode *response = xml_node_child_at(multistatus, i);
      XMLNode *href = xml_node_find_tag(response, "href", false);
      char url[strlen(calendar->client->base_url) + strlen(href->text) + 1];
      sprintf(url, "%s%s", calendar->client->base_url, href->text);
      XMLNode *cal_data = xml_node_find_tag(response, "calendar-data", false);
      CalDAVEvent *event = caldav_event_new(cal_data->text, url);
      caldav_list_add(output, event);
    }
    xml_node_free(root);
  }
  CALDAV_FREE(response.data);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return output;
}

CalDAVEvent *caldav_calendar_create_event(CalDAVCalendar *calendar, const char *ical) {
  CALDAV_LOG("Create event in calendar '%s'", calendar->name);
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
  char *uuid = caldav_ical_get_prop(ical, "UID");
  char *url = strdup_printf("%s%s.ics", calendar->client->calendars_url, uuid);
  char *request_body;
  if (!strstr(ical, "BEGIN:VCALENDAR"))
    request_body =
        strdup_printf("BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:-//Errands//EN\n%sEND:VCALENDAR", ical);
  else
    request_body = strdup(ical);
  CURLcode res;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: text/calendar");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
  curl_easy_setopt(curl, CURLOPT_USERPWD, calendar->client->usrpwd);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  CalDAVEvent *event = NULL;
  res = curl_easy_perform(curl);
  if (res != CURLE_OK)
    CALDAV_LOG("PUT request failed: %s\n", curl_easy_strerror(res));
  else {
    CalDAVEvent *event = caldav_event_new(request_body, url);
  }
  CALDAV_FREE(uuid);
  CALDAV_FREE(url);
  CALDAV_FREE(request_body);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
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

CalDAVEvent *caldav_event_new(const char *ical, const char *url) {
  CalDAVEvent *event = malloc(sizeof(CalDAVEvent));
  event->ical = strdup(ical);
  event->url = strdup(url);
  return event;
}

void caldav_event_set_url(CalDAVEvent *event, const char *url) {
  if (event->url)
    CALDAV_FREE(event->url);
  event->url = strdup(url);
}

const char *caldav_event_get_url(CalDAVEvent *event) { return event->url; }

void caldav_event_set_ical(CalDAVEvent *event, const char *ical) {
  if (event->ical)
    CALDAV_FREE(event->ical);
  event->ical = strdup(ical);
}

const char *caldav_event_get_ical(CalDAVEvent *event) { return event->ical; }

bool caldav_event_pull(CalDAVClient *client, CalDAVEvent *event) { return true; }

bool caldav_event_push(CalDAVClient *client, CalDAVEvent *event) { return true; }

void caldav_event_print(CalDAVEvent *event) {
  CALDAV_LOG("Event at %s:\n%s", event->url, event->ical);
}

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

size_t caldav_list_length(CalDAVList *list) { return list->len; }

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
