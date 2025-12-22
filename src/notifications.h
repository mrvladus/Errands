#pragma once

#include "data.h"

// Initialize notifications system
void errands_notifications_init(void);
// Start sending notification
void errands_notifications_start(void);
// Stop sending notification
void errands_notifications_stop(void);
// Add a task to notifications queue
void errands_notifications_add(ErrandsData *data);
void errands_notifications_send();
// Cleanup notifications system
void errands_notifications_cleanup(void);
