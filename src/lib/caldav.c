#include "caldav.h"
// #define XML_H_DEBUG
#include "xml.h"

#include <curl/curl.h>

#include <regex.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG(format, ...) fprintf(stderr, "[CalDAV] " format "\n", ##__VA_ARGS__)

char *caldav_discover_caldav_url();
char *caldav_get_principal_url();

char *__usrpwd = NULL;
char *__base_url = NULL;
char *__caldav_url = NULL;
char *__principal_url = NULL;
char *__calendars_url = NULL;

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
size_t write_callback(void *ptr, size_t size, size_t nmemb, struct response_data *data) {
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

char *caldav_propfind(const char *url, int depth, const char *xml) {
  CURL *curl;
  CURLcode res;
  struct response_data response = {NULL, 0};

  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url);
    // Set the HTTP method to PROPFIND
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
    // Set the request headers
    struct curl_slist *headers = NULL;
    char __depth[10];
    sprintf(__depth, "Depth: %d", depth);
    headers = curl_slist_append(headers, __depth);
    headers = curl_slist_append(headers, "Prefer: return-minimal");
    headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml);
    curl_easy_setopt(curl, CURLOPT_USERPWD, __usrpwd);
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

char *caldav_discover_caldav_url() {
  printf("Discover CalDAV URL ... ");

  char *out = NULL;

  CURL *curl = curl_easy_init();
  if (!curl)
    return out;

  // Get discover url
  char discover_url[strlen(__base_url) + 20];
  sprintf(discover_url, "%s/.well-known/caldav", __base_url);
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

  printf("%s\n", out);
  return out;
}

char *caldav_get_principal_url() {
  printf("Getting CalDAV principal URL ... ");

  char *out = NULL;
  const char *request_body = "<d:propfind xmlns:d=\"DAV:\">"
                             "  <d:prop>"
                             "    <d:current-user-principal />"
                             "  </d:prop>"
                             "</d:propfind>";
  char *xml = caldav_propfind(__caldav_url, 0, request_body);
  XMLNode *root = xml_parse_string(xml);
  XMLNode *href = xml_node_find_by_path(
      root, "multistatus/response/propstat/prop/current-user-principal/href", false);

  char url[strlen(href->text) + strlen(__base_url) + 1];
  sprintf(url, "%s%s", __base_url, href->text);
  out = strdup(url);

  free(xml);
  xml_node_free(root);

  printf("%s\n", out);
  return out;
}

char *caldav_get_calendars_url() {
  printf("Getting CalDAV calendars URL ... ");

  char *out = NULL;
  const char *request_body =
      "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
      "  <d:prop>"
      "    <c:calendar-home-set />"
      "  </d:prop>"
      "</d:propfind>";
  char *xml = caldav_propfind(__principal_url, 0, request_body);
  XMLNode *root = xml_parse_string(xml);
  XMLNode *href = xml_node_find_by_path(
      root, "multistatus/response/propstat/prop/calendar-home-set/href", false);

  char url[strlen(href->text) + strlen(__base_url) + 1];
  sprintf(url, "%s%s", __base_url, href->text);
  out = strdup(url);

  free(xml);
  xml_node_free(root);

  printf("%s\n", out);
  return out;
}

CalDAVCalendar *caldav_calendar_new() { return (CalDAVCalendar *)malloc(sizeof(CalDAVCalendar)); }

CalDAVList *caldav_get_calendars() {
  const char *request_body =
      "<d:propfind xmlns:d=\"DAV:\" "
      "xmlns:cs=\"http://calendarserver.org/ns/\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
      "  <d:prop>"
      "    <d:resourcetype />"
      "    <d:displayname/>"
      "    <cs:getctag />"
      "    <c:supported-calendar-component-set />"
      "  </d:prop>"
      "</d:propfind>";
  char *xml = caldav_propfind(__calendars_url, 1, request_body);
  CalDAVList *calendars_list = caldav_list_new();

  XMLNode *root = xml_parse_string(xml);
  XMLNode *multistatus = xml_node_child_at(root, 0);
  for (size_t i = 0; i < multistatus->children->len; i++) {
    XMLNode *response = xml_node_child_at(multistatus, i);
    XMLNode *supported_set = xml_node_find_tag(response, "supported-calendar-component-set", false);
    if (supported_set) {
      const char *set = xml_node_attr(xml_node_child_at(supported_set, 0), "name");
      if (!strcmp(set, "VTODO")) {
        XMLNode *deleted = xml_node_find_tag(response, "deleted-calendar", false);
        if (!deleted) {
          CalDAVCalendar *calendar = caldav_calendar_new();

          XMLNode *name = xml_node_find_tag(response, "displayname", false);
          calendar->name = strdup(name->text);

          XMLNode *href = xml_node_find_tag(response, "href", false);
          char cal_url[strlen(__base_url) + strlen(href->text) + 1];
          snprintf(cal_url, strlen(__base_url) + strlen(href->text) + 1, "%s%s", __base_url,
                   href->text);
          calendar->url = strdup(cal_url);
          calendar->uuid = __extract_uuid(href->text);
          caldav_list_add(calendars_list, calendar);

          printf("Found calendar '%s' at %s with uuid='%s'\n", calendar->name, calendar->url,
                 calendar->uuid);
        }
      }
    }
  }
  xml_node_free(root);

  return calendars_list;
}

// ---------- INIT / CLEANUP ---------- //

void caldav_init(const char *base_url, const char *username, const char *password) {
  printf("Init CalDAV\n");

  curl_global_init(CURL_GLOBAL_DEFAULT);
  char usrpwd[strlen(username) + strlen(password) + 1];
  sprintf(usrpwd, "%s:%s", username, password);
  __usrpwd = strdup(usrpwd);
  __base_url = strdup(base_url);
  __caldav_url = caldav_discover_caldav_url();
  __principal_url = caldav_get_principal_url();
  __calendars_url = caldav_get_calendars_url();
  caldav_get_calendars();
}

void caldav_cleanup() {
  free(__usrpwd);
  free(__base_url);
  free(__caldav_url);
  free(__principal_url);
  curl_global_cleanup();
}

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

void caldav_list_free(CalDAVList *list) {
  if (list != NULL) {
    free(list->data);
    free(list);
  }
}
