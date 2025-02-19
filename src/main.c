#include "config.h"
#include "data.h"
#include "settings.h"
#include "state.h"
#include "sync.h"
#include "utils.h"
#include "window.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <libical/ical.h>
#include <libportal/portal.h>

extern GResource *errands_get_resource(void);

static void activate(GtkApplication *app) {
  state.main_window = errands_window_new();
  errands_window_build(state.main_window);
  gtk_window_present(GTK_WINDOW(state.main_window));
  sync_init();
}

int main(int argc, char **argv) {
  state.is_flatpak = xdp_portal_running_under_flatpak();
  LOG("Starting Errands %s %s", VERSION, state.is_flatpak ? "(flatpak)" : "(not official)");
  // Generate random seed
  srand((unsigned int)(time(NULL) ^ getpid()));

  bindtextdomain("errands", LOCALE_DIR);
  bind_textdomain_codeset("errands", "UTF-8");
  textdomain("errands");

  errands_settings_init();
  errands_data_load_lists();

  state.app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(state.app, "activate", G_CALLBACK(activate), NULL);
  g_resources_register(errands_get_resource());
  int status = g_application_run(G_APPLICATION(state.app), argc, argv);
  g_object_unref(state.app);
  return status;
}
