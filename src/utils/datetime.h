#pragma once

#include "macros.h"
#include <glib.h>
#include <stdio.h>

static inline void get_date_time(char *date_time) {
  GDateTime *dt = g_date_time_new_now_local();
  char *tmp = g_date_time_format(dt, "%Y%m%dT%H%M%SZ");
  strcpy(date_time, tmp);
  g_free(tmp);
  g_date_time_unref(dt);
}

static inline char *get_today_date() {
  GDateTime *dt = g_date_time_new_now_local();
  static char date[9];
  char *tmp = g_date_time_format(dt, "%Y%m%d");
  strcpy(date, tmp);
  g_free(tmp);
  g_date_time_unref(dt);
  return date;
}

// Helper function to parse the due date into a GDateTime
static inline GDateTime *parse_date(const char *date) {
  if (strlen(date) == 8) { // Format: "YYYYMMDD"
    char year[5], month[3], day[3];
    strncpy(year, date, 4);
    strncpy(month, date + 4, 2);
    strncpy(day, date + 6, 2);
    year[4] = '\0';
    month[2] = '\0';
    day[2] = '\0';
    char dt[17];
    sprintf(dt, "%s%s%sT000000Z", year, month, day);
    return g_date_time_new_from_iso8601(dt, NULL);
  } else if (strlen(date) == 16) { // Format: "YYYYMMDDTHHMMSSZ"
    return g_date_time_new_from_iso8601(date, NULL);
  }
  return NULL;
}
