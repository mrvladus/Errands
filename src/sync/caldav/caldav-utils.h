#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline void caldav_log(const char *format, ...) {
  va_list args;
  printf("[CalDAV] ");
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  printf("\n");
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
  va_list args;
  va_start(args, fmt);
  char *str = NULL;
  vasprintf(&str, fmt, args);
  va_end(args);
  return str;
}

// Function to extract the base URL from a given URL string.
// The returned string is dynamically allocated and should be freed by the caller.
static inline char *extract_base_url(const char *url) {
  if (!url)
    return NULL;
  const char *scheme_end = strstr(url, "://"); // Pointer to find the "://"
  size_t base_len = 0;
  if (scheme_end) {
    base_len = (scheme_end - url) + strlen("://");          // Include scheme and "://"
    const char *host_start = scheme_end + strlen("://");    // The start of the host (base URL) part.
    const char *slash_after_host = strchr(host_start, '/'); // Find the next '/' after the host start.
    if (slash_after_host)
      base_len += (slash_after_host - host_start); // Calculate overall length: scheme + "://" + host
    else
      base_len = strlen(url); // No slash after host, use the entire string.
  } else {
    const char *slash = strchr(url, '/'); // No scheme, consider the entire string up to the first slash, if any.
    base_len = slash ? slash - url : strlen(url);
  }
  char *base_url = (char *)malloc(base_len + 1); // Allocate memory for the base URL string (+1 for null terminator)
  if (!base_url)
    return NULL;
  strncpy(base_url, url, base_len); // Copy the base URL part.
  base_url[base_len] = '\0';        // Null terminate the string
  return base_url;
}

// Generate a UUIDv4 string.
// The returned string is dynamically allocated and should be freed by the caller.
static inline char *generate_uuid4() {
  srand((unsigned int)time(NULL));
  const char *hex_chars = "0123456789abcdef";
  char *uuid = malloc(37); // Allocate 36 characters + null terminator
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

// Extract last part of the URL path
static inline char *extract_uuid(const char *url) {
  if (!url || *url == '\0')
    return strdup(""); // Return an empty string if the path is empty
  size_t len = strlen(url);
  size_t end = len - 1;
  if (url[end] == '/')
    end--;
  size_t start = end;
  while (url[start] != '/')
    start--;
  start++;
  char *out = malloc(end - start + 1);
  strncpy(out, url + start, end - start);
  out[end - start] = '\0';
  return out;
}

// Generate a random hexadecimal color code.
// The returned string is dynamically allocated and should be freed by the caller.
static inline char *caldav_generate_hex_color() {
  char *color = malloc(8);
  if (color == NULL)
    return NULL;
  srand(time(NULL));
  int r = rand() % 256;
  int g = rand() % 256;
  int b = rand() % 256;
  sprintf(color, "#%02X%02X%02X", r, g, b);
  return color;
}

// Get ical prop
static inline char *caldav_ical_get_prop(const char *ical, const char *prop) {
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
