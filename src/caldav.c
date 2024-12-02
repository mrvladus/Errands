#include "caldav.h"

#include <stdlib.h>
#include <string.h>

// No-op callback to discard response body
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data) {
  return size * nmemb; // Ignore the data
}

char *caldav_discover_caldav_url(const char *base_url) {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL *curl = curl_easy_init();
  if (!curl)
    return NULL;

  // Get discover url
  const char discover_suffix[] = "/.well-known/caldav";
  char discover_url[strlen(base_url) + strlen(discover_suffix) + 1];
  sprintf(discover_url, "%s%s", base_url, discover_suffix);

  // Set the URL you want to fetch
  curl_easy_setopt(curl, CURLOPT_URL, discover_url);
  // Follow redirects (like `-L` in curl)
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  // Suppress output (equivalent to `-o /dev/null` in curl)
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

  // Perform the request
  CURLcode res = curl_easy_perform(curl);
  if (res == CURLE_OK) {
    char *effective_url;
    // Get the final URL after any redirects
    res = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (res == CURLE_OK) {
      char *redirect_url = malloc(strlen(effective_url) + 1);
      strcpy(redirect_url, effective_url);
      return redirect_url;
    }
  }

  // Cleanup
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return NULL;
}
