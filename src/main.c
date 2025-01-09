#include "config.h"
#include "data.h"
#include "glib.h"
#include "resources.h"
#include "settings.h"
#include "state.h"
#include "sync.h"
#include "utils/files.h"
#include "utils/macros.h"
#include "window.h"

#include <adwaita.h>
#include <glib/gi18n.h>

#include <stddef.h>

static void activate(GtkApplication *app) {
  state.main_window = errands_window_new();
  errands_window_build(state.main_window);
  gtk_window_present(GTK_WINDOW(state.main_window));
  sync_init();
}

int main(int argc, char **argv) {
  CalendarData_array a = errands_data_load_calendars();
  // for (size_t i = 0; i < a.len; i++) {
  //   CalendarData d = a.data[i];
  //   LOG("uid: %s, deleted: %d", d.uid, calendar_data_get_deleted(&d));
  // }

  // EventData_array e = errands_data_load_events(&a);
  // for (size_t i = 0; i < e.len; i++) {
  //   LOG("%s", event_data_get_text(&e.data[i]));
  // }

  // Generate random seed
  srand((unsigned int)(time(NULL) ^ getpid()));

  bindtextdomain("errands", LOCALE_DIR);
  bind_textdomain_codeset("errands", "UTF-8");
  textdomain("errands");

  errands_data_load();
  errands_settings_init();

  state.app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(state.app, "activate", G_CALLBACK(activate), NULL);
  g_resources_register(errands_get_resource());
  int status = g_application_run(G_APPLICATION(state.app), argc, argv);
  g_object_unref(state.app);
  return status;
}
