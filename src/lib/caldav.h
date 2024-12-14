#pragma once
#include <stddef.h>
#include <stdlib.h>

// Initialize caldav client with url, username and password
void caldav_init(const char *base_url, const char *username, const char *password);
// Cleanup all the memory allocations
void caldav_cleanup();

typedef struct CalDAVList {
  size_t size;
  size_t len;
  void **data;
} CalDAVList;

CalDAVList *caldav_list_new();
void caldav_list_add(CalDAVList *list, void *data);
void caldav_list_free(CalDAVList *list);

typedef struct CalDAVCalendar {
  char *url;
  char *name;
  char *uuid;
} CalDAVCalendar;

CalDAVCalendar *caldav_calendar_new();
// Fetch calendars from server
CalDAVList *caldav_get_calendars();
