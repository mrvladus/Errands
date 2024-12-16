#include "caldav.h"
// #define XML_H_DEBUG
#include "xml.h"

#include <curl/curl.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALDAV_LOG(format, ...) printf("[CalDAV] " format "\n", ##__VA_ARGS__)

// --- STRUCTS --- //

struct _CalDAVClient {
  char *usrpwd;
  char *base_url;
  char *caldav_url;
  char *principal_url;
  char *calendars_url;
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

char *__extract_uuid(const char *path) {
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

// ---------- REQUESTS ---------- //

struct response_data {
  char *data;
  size_t size;
};

// Callback function to write the response data
static size_t write_callback(void *ptr, size_t size, size_t nmemb, struct response_data *data) {
  size_t total_size = size * nmemb;
  char *temp = realloc(data->data, data->size + total_size + 1);
  if (temp == NULL) {
    // Out of memory
    return 0;
  }
  data->data = temp;
  memcpy(&(data->data[data->size]), ptr, total_size);
  data->size += total_size;
  data->data[data->size] = '\0'; // Null-terminate the string
  return total_size;
}

static char *caldav_request(CalDAVClient *client, const char *method, const char *url, size_t depth,
                            const char *xml) {
  CURL *curl;
  CURLcode res;
  struct response_data response = {NULL, 0};

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    // Set the HTTP method to PROPFIND
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    // Set the request headers
    struct curl_slist *headers = NULL;
    char __depth[10];
    sprintf(__depth, "Depth: %d", (int)depth);
    headers = curl_slist_append(headers, __depth);
    headers = curl_slist_append(headers, "Prefer: return-minimal");
    headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml);
    curl_easy_setopt(curl, CURLOPT_USERPWD, client->usrpwd);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    // Perform the request
    res = curl_easy_perform(curl);
    // Check for errors
    if (res != CURLE_OK) {
      free(response.data);
      response.data = NULL;
    }
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
  }

  return response.data; // Caller is responsible for freeing the returned string
}

// No-op callback to discard response body
static size_t null_write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
  return size * nmemb; // Ignore the data
}

static char *caldav_discover_caldav_url(CalDAVClient *client) {
  CALDAV_LOG("Discover CalDAV URL...");
  char *out = NULL;
  CURL *curl = curl_easy_init();
  if (!curl)
    return out;
  // Get discover url
  char discover_url[strlen(client->base_url) + 20];
  sprintf(discover_url, "%s/.well-known/caldav", client->base_url);
  curl_easy_setopt(curl, CURLOPT_URL, discover_url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  // Perform the request
  CURLcode res = curl_easy_perform(curl);
  if (res == CURLE_OK) {
    char *effective_url;
    res = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (res == CURLE_OK)
      out = strdup(effective_url);
  }
  curl_easy_cleanup(curl);
  CALDAV_LOG("%s", out);
  return out;
}

// ---------- CLIENT ---------- //

static char *caldav_client_get_principal_url(CalDAVClient *client) {
  CALDAV_LOG("Getting CalDAV principal URL...");
  char *out = NULL;
  const char *request_body = "<d:propfind xmlns:d=\"DAV:\">"
                             "  <d:prop>"
                             "    <d:current-user-principal />"
                             "  </d:prop>"
                             "</d:propfind>";
  char *xml = caldav_request(client, "PROPFIND", client->caldav_url, 0, request_body);
  XMLNode *root = xml_parse_string(xml);
  XMLNode *href = xml_node_find_by_path(
      root, "multistatus/response/propstat/prop/current-user-principal/href", false);
  char url[strlen(href->text) + strlen(client->base_url) + 1];
  sprintf(url, "%s%s", client->base_url, href->text);
  out = strdup(url);
  free(xml);
  xml_node_free(root);
  CALDAV_LOG("%s", out);
  return out;
}

static char *caldav_client_get_calendars_url(CalDAVClient *client) {
  CALDAV_LOG("Getting CalDAV calendars URL...");
  char *out = NULL;
  const char *request_body =
      "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
      "  <d:prop>"
      "    <c:calendar-home-set />"
      "  </d:prop>"
      "</d:propfind>";
  char *xml = caldav_request(client, "PROPFIND", client->principal_url, 0, request_body);
  XMLNode *root = xml_parse_string(xml);
  XMLNode *href = xml_node_find_by_path(
      root, "multistatus/response/propstat/prop/calendar-home-set/href", false);
  char url[strlen(href->text) + strlen(client->base_url) + 1];
  sprintf(url, "%s%s", client->base_url, href->text);
  out = strdup(url);
  free(xml);
  xml_node_free(root);
  CALDAV_LOG("%s", url);
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
  client->caldav_url = caldav_discover_caldav_url(client);
  if (!client->caldav_url)
    return NULL;
  client->principal_url = caldav_client_get_principal_url(client);
  client->calendars_url = caldav_client_get_calendars_url(client);
  return client;
}

CalDAVList *caldav_client_get_calendars(CalDAVClient *client, const char *set) {
  CALDAV_LOG("Getting calendars with '%s'...", set);
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
  char *xml = caldav_request(client, "PROPFIND", client->calendars_url, 1, request_body);
  CalDAVList *calendars_list = caldav_list_new();
  XMLNode *root = xml_parse_string(xml);
  XMLNode *multistatus = xml_node_child_at(root, 0);
  for (size_t i = 0; i < multistatus->children->len; i++) {
    XMLNode *response = xml_node_child_at(multistatus, i);
    XMLNode *supported_set = xml_node_find_tag(response, "supported-calendar-component-set", false);
    if (supported_set) {
      const char *set_val = xml_node_attr(xml_node_child_at(supported_set, 0), "name");
      if (!strcmp(set, "VTODO") && !strcmp(set_val, "VTODO")) {
        XMLNode *deleted = xml_node_find_tag(response, "deleted-calendar", false);
        if (!deleted) {
          XMLNode *name = xml_node_find_tag(response, "displayname", false);
          XMLNode *color = xml_node_find_tag(response, "calendar-color", false);
          XMLNode *href = xml_node_find_tag(response, "href", false);
          char cal_url[strlen(client->base_url) + strlen(href->text) + 1];
          snprintf(cal_url, strlen(client->base_url) + strlen(href->text) + 1, "%s%s",
                   client->base_url, href->text);
          CalDAVCalendar *calendar =
              caldav_calendar_new(color->text, (char *)set, name->text, cal_url);
          caldav_list_add(calendars_list, calendar);
          caldav_calendar_print(calendar);
        }
      }
    }
  }
  free(xml);
  xml_node_free(root);

  return calendars_list;
}

void caldav_client_free(CalDAVClient *client) {
  if (client->usrpwd)
    free(client->usrpwd);
  if (client->base_url)
    free(client->base_url);
  if (client->caldav_url)
    free(client->caldav_url);
  if (client->principal_url)
    free(client->principal_url);
  free(client);
  curl_global_cleanup();
}

// ---------- CALENDAR ---------- //

CalDAVCalendar *caldav_calendar_new(char *color, char *set, char *name, char *url) {
  CalDAVCalendar *calendar = (CalDAVCalendar *)malloc(sizeof(CalDAVCalendar));
  calendar->color = color;
  calendar->set = set;
  calendar->name = strdup(name);
  calendar->url = strdup(url);
  calendar->uuid = __extract_uuid(url);
  return calendar;
}

CalDAVList *caldav_calendar_get_events(CalDAVClient *client, CalDAVCalendar *calendar) {
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
  char *xml = caldav_request(client, "REPORT", calendar->url, 1, request_body);
  XMLNode *root = xml_parse_string(xml);
  CalDAVList *events_list = caldav_list_new();
  XMLNode *multistatus = xml_node_child_at(root, 0);
  for (size_t i = 0; i < multistatus->children->len; i++) {
    XMLNode *response = xml_node_child_at(multistatus, i);
    XMLNode *href = xml_node_find_tag(response, "href", false);
    char url[strlen(client->base_url) + strlen(href->text) + 1];
    sprintf(url, "%s%s", client->base_url, href->text);
    XMLNode *cal_data = xml_node_find_tag(response, "calendar-data", false);
    CalDAVEvent *event = caldav_event_new(cal_data->text, url);
    caldav_list_add(events_list, event);
  }
  free(xml);
  xml_node_free(root);
  return events_list;
}

void caldav_calendar_print(CalDAVCalendar *calendar) {
  CALDAV_LOG("Calendar '%s' at %s with color '%s'", calendar->name, calendar->url,
             calendar->color ? calendar->color : "");
}

void caldav_calendar_free(CalDAVCalendar *calendar) {
  free(calendar->name);
  free(calendar->set);
  free(calendar->url);
  free(calendar->uuid);
  free(calendar);
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
    free(event->url);
  event->url = strdup(url);
}

const char *caldav_event_get_url(CalDAVEvent *event) { return event->url; }

void caldav_event_set_ical(CalDAVEvent *event, const char *ical) {
  if (event->ical)
    free(event->ical);
  event->ical = strdup(ical);
}

const char *caldav_event_get_ical(CalDAVEvent *event) { return event->ical; }

bool caldav_event_pull(CalDAVClient *client, CalDAVEvent *event) { return true; }

bool caldav_event_push(CalDAVClient *client, CalDAVEvent *event) { return true; }

void caldav_event_print(CalDAVEvent *event) {
  CALDAV_LOG("Event at %s:\n%s\n", event->url, event->ical);
}

void caldav_event_free(CalDAVEvent *event) {
  free(event->ical);
  free(event->url);
  free(event);
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
    free(list);
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
    free(list->data);
    free(list);
  }
}
