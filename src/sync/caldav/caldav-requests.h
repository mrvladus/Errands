#pragma once

#include "caldav.h"

char *caldav_autodiscover(CalDAVClient *client);
char *caldav_propfind(CalDAVClient *client, const char *url, size_t depth, const char *body);
bool caldav_delete(CalDAVClient *client, const char *url);
char *caldav_get(CalDAVClient *client, const char *url);
bool caldav_put(CalDAVClient *client, const char *url, const char *body);
char *caldav_report(CalDAVClient *client, const char *url, const char *body);
bool caldav_mkcalendar(CalDAVClient *client, const char *url, const char *name, const char *color,
                       CalDAVComponentSet set);
