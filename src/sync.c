#include "sync.h"
#include "lib/caldav.h"

void sync_init() {
  caldav_init("http://127.0.0.1:8080", "vlad", "1710");
  // Get calendars
}
