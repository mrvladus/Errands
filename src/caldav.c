#include "caldav.h"

#include <curl/curl.h>

#include <regex.h>
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

char *get_tag(const char *xml, const char *regex) {
  regex_t compiled_regex;
  regmatch_t matches[2];

  // Compile the regex
  if (regcomp(&compiled_regex, regex, REG_EXTENDED) != 0) {
    fprintf(stderr, "Could not compile regex\n");
    return NULL;
  }

  // Execute the regex
  if (regexec(&compiled_regex, xml, 2, matches, 0) == 0) {
    // Extract the matched href
    size_t start = matches[1].rm_so; // Start of the match
    size_t end = matches[1].rm_eo;   // End of the match

    // Calculate the length of the matched string
    size_t length = end - start;

    // Allocate memory for the matched string
    char *href = (char *)malloc(length + 1);
    if (href == NULL) {
      fprintf(stderr, "Memory allocation failed\n");
      regfree(&compiled_regex);
      return NULL;
    }

    // Copy the matched string into the allocated memory
    strncpy(href, xml + start, length);
    href[length] = '\0'; // Null-terminate the string

    // Free the compiled regex
    regfree(&compiled_regex);
    return href; // Return the matched href
  } else {
    // Free the compiled regex
    regfree(&compiled_regex);
    return NULL; // No match found
  }
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

  char *xml = caldav_propfind(
      __caldav_url, 0,
      "<d:propfind xmlns:d=\"DAV:\"><d:prop><d:current-user-principal /></d:prop></d:propfind>");
  char *principal_url = get_tag(xml, "<[^:]+:current-user-principal>\\s*<[^:]+:href>(.*?)</"
                                     "[^:]+:href>\\s*</[^:]+:current-user-principal>");
  char url[strlen(principal_url) + strlen(__base_url) + 1];
  sprintf(url, "%s%s", __base_url, principal_url);
  out = strdup(url);

  free(xml);
  free(principal_url);

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
  char *calendar_url = get_tag(xml, "<[^:]+:calendar-home-set>\\s*<[^:]+:href>(.*?)</"
                                    "[^:]+:href>\\s*</[^:]+:calendar-home-set>");
  char url[strlen(calendar_url) + strlen(__base_url) + 1];
  sprintf(url, "%s%s", __base_url, calendar_url);
  out = strdup(url);

  free(xml);
  free(calendar_url);

  printf("%s\n", out);
  return out;
}

char *caldav_get_calendars() {
  char *out = NULL;
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
  printf("%s\n", xml);
  return out;
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
