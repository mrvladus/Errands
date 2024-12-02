#include "sync.h"
#include "caldav.h"
#include "utils.h"

char *base_url;

void sync_init() {
  base_url = caldav_discover_caldav_url("http://127.0.0.1:8080/");
  if (!base_url) {
    LOG("Sync: Failed to discover caldav URL");
    return;
  }
  LOG("Sync: Set caldav base url to %s", base_url);
}
