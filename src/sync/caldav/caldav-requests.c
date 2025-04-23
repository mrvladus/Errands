#include "caldav-requests.h"
#include "caldav-utils.h"

#include <curl/curl.h>

#include <stdlib.h>
#include <string.h>

struct response_data {
  char *data;
  size_t size;
};

// Callback function to write the response data
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
  struct response_data *_data = (struct response_data *)data;
  size_t total_size = size * nmemb;
  char *temp = realloc(_data->data, _data->size + total_size + 1);
  if (temp == NULL)
    return 0;
  _data->data = temp;
  memcpy(&(_data->data[_data->size]), ptr, total_size);
  _data->size += total_size;
  _data->data[_data->size] = '\0';
  return total_size;
}

// No-op callback to discard response body
static size_t null_write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
  return size * nmemb; // Ignore the data
}

char *caldav_autodiscover(CalDAVClient *client) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
  char *caldav_url = NULL;
  char discover_url[strlen(client->base_url) + 20];
  sprintf(discover_url, "%s/.well-known/caldav", client->base_url);
  curl_easy_setopt(curl, CURLOPT_URL, discover_url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, null_write_callback);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    caldav_log("Failed to get CalDAV url: %s", curl_easy_strerror(res));
    return false;
  } else {
    char *effective_url = NULL;
    res = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (res == CURLE_OK && effective_url) {
      caldav_url = strdup(effective_url);
      caldav_log("CalDAV URL: %s", caldav_url);
    } else {
      caldav_log("Failed to retrieve effective URL.");
      return false;
    }
  }
  curl_easy_cleanup(curl);
  return caldav_url;
}

char *caldav_propfind(CalDAVClient *client, const char *url, size_t depth, const char *body) {
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
  curl_easy_setopt(curl, CURLOPT_USERNAME, client->username);
  curl_easy_setopt(curl, CURLOPT_PASSWORD, client->password);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    caldav_log("%s", curl_easy_strerror(res));
    free(response.data);
    response.data = NULL;
  }
  caldav_free(__depth);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return response.data;
}

bool caldav_delete(CalDAVClient *client, const char *url) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return false;
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

char *caldav_get(CalDAVClient *client, const char *url) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
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
    free(response.data);
  }
  curl_easy_cleanup(curl);
  return response.data;
}

bool caldav_put(CalDAVClient *client, const char *url, const char *body) {
  bool out = false;
  CURL *curl = curl_easy_init();
  if (!curl)
    return out;
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
  if (res != CURLE_OK)
    caldav_log("%s", curl_easy_strerror(res));
  else
    out = true;
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return out;
}

char *caldav_report(CalDAVClient *client, const char *url, const char *body) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;
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
  if (res != CURLE_OK)
    caldav_log("%s", curl_easy_strerror(res));
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  return response.data;
}

bool caldav_mkcalendar(CalDAVClient *client, const char *url, const char *name, const char *color,
                       CalDAVComponentSet set) {
  CURL *curl = curl_easy_init();
  if (!curl)
    return false;
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
  if (set & CALDAV_COMPONENT_SET_VTODO)
    strcat(comp_set, "<c:comp name=\"VTODO\"/>");
  if (set & CALDAV_COMPONENT_SET_VEVENT)
    strcat(comp_set, "<c:comp name=\"VEVENT\"/>");
  if (set & CALDAV_COMPONENT_SET_VJOURNAL)
    strcat(comp_set, "<c:comp name=\"VJOURNAL\"/>");

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
