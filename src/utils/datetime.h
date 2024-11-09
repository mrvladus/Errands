#pragma once

#include <glib.h>

static inline char *get_date_time() {
  GDateTime *dt = g_date_time_new_now_local();
  static char datetime[17];
  char *tmp = g_date_time_format(dt, "%Y%m%dT%H%M%SZ");
  strcpy(datetime, tmp);
  g_free(tmp);
  g_date_time_unref(dt);
  return datetime;
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
