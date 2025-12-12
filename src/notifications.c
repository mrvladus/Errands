#include "notifications.h"
#include "data.h"
#include "glib.h"
#include "settings.h"
#include "state.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libical/ical.h>
#include <stddef.h>

static const uint8_t check_interval = 10;
static GPtrArray *queue = NULL;
static GPtrArray *save_lists = NULL;
static bool initialized = false;
static bool sending = false;

static void send_due(const char *text) {
  if (!text) return;
  g_autoptr(GNotification) notification = g_notification_new(_("Task is Due"));
  g_autoptr(GIcon) icon = g_themed_icon_new("object-select-symbolic");
  g_notification_set_body(notification, text);
  g_notification_set_icon(notification, icon);
  g_application_send_notification(G_APPLICATION(state.app), NULL, notification);
}

static bool notify_cb() {
  if (!sending || !errands_settings_get(SETTING_NOTIFICATIONS).b) return false;
  size_t sended = 0;
  for (int i = queue->len - 1; i >= 0; i--) {
    TaskData *data = g_ptr_array_index(queue, i);
    if (errands_task_data_is_due(data)) {
      g_ptr_array_remove_index(queue, i);
      errands_data_set(data->data, DATA_PROP_NOTIFIED, true);
      if (!g_ptr_array_find(save_lists, data->list, NULL)) g_ptr_array_add(save_lists, data->list);
      send_due(errands_data_get_str(data->data, DATA_PROP_TEXT));
      sended++;
    }
  }
  // Save lists
  for_range(i, 0, save_lists->len) errands_data_write_list(g_ptr_array_index(save_lists, i));
  g_ptr_array_set_size(save_lists, 0);
  if (sended > 0) LOG("Notifications: Sent %zu notifications", sended);
  return true;
}

// Initialize notifications system
void errands_notifications_init(void) {
  if (initialized) return;
  if (!errands_settings_get(SETTING_NOTIFICATIONS).b) return;
  LOG("Notifications: Initialize");
  TIMER_START;
  queue = g_ptr_array_new();
  save_lists = g_ptr_array_new();
  // Add all due tasks which are not notified yet to the queue
  g_autoptr(GPtrArray) tasks = g_ptr_array_new();
  errands_data_get_flat_list(tasks);
  for_range(i, 0, tasks->len) {
    TaskData *data = g_ptr_array_index(tasks, i);
    bool has_due_date = !icaltime_is_null_time(errands_data_get_time(data->data, DATA_PROP_DUE_TIME));
    if (has_due_date && !errands_data_get_bool(data->data, DATA_PROP_NOTIFIED)) errands_notifications_add(data);
  }
  initialized = true;
  LOG("Notifications: Added %d tasks to the notifications queue (%f sec.)", queue->len, TIMER_ELAPSED_MS);
}

// Start sending notification
void errands_notifications_start(void) {
  if (!initialized) errands_notifications_init();
  if (!initialized) return;
  sending = true;
  g_timeout_add_seconds(check_interval, G_SOURCE_FUNC(notify_cb), NULL);
  LOG("Notifications: Started sending notifications");
}

// Stop sending notification
void errands_notifications_stop(void) {
  sending = false;
  LOG("Notifications: Stopped sending notifications");
}

// Add a task to notifications queue
void errands_notifications_add(TaskData *data) {
  if (!data || g_ptr_array_find(queue, data, NULL)) return;
  g_ptr_array_add(queue, data);
  LOG("Notifications: Added task '%s' to the notifications queue", errands_data_get_str(data->data, DATA_PROP_UID));
}

// Cleanup notifications system
void errands_notifications_cleanup(void) {
  LOG("Notifications: Cleanup");
  g_ptr_array_free(queue, true);
  g_ptr_array_free(save_lists, true);
  queue = NULL;
  save_lists = NULL;
}
