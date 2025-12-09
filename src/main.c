#if !(defined(__GNUC__) || defined(__clang__))
#error "This code requires GCC or Clang compiler because it uses features not supported by other compilers.\
e.g. GLib's g_autoptr, g_auto and g_autofree"
#endif

#include "config.h"
#include "data.h"
#include "settings.h"
#include "state.h"
#include "window.h"

#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libportal/portal.h>

extern GResource *errands_get_resource(void);
TOOLBOX_TMP_STR;
const char *toolbox_log_prefix = "\033[0;32m[Errands] \033[0m";
State state = {0};

static void activate(GtkApplication *app) {
  state.main_window = errands_window_new(app);
  errands_sidebar_load_lists(ERRANDS_SIDEBAR(state.main_window->sidebar));
  gtk_window_present(GTK_WINDOW(state.main_window));
  // sync_init();
}

int main(int argc, char **argv) {
  LOG("Starting Errands %s (%s) %sFlatpak", VERSION, VERSION_COMMIT, xdp_portal_running_under_flatpak() ? "" : "not ");
  // Generate random seed
  srand((unsigned int)(TIME_NOW ^ getpid()));

  bindtextdomain("errands", LOCALE_DIR);
  bind_textdomain_codeset("errands", "UTF-8");
  textdomain("errands");

  errands_settings_init();
  errands_data_init();

  state.app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(state.app, "activate", G_CALLBACK(activate), NULL);
  g_resources_register(errands_get_resource());
  const int status = g_application_run(G_APPLICATION(state.app), argc, argv);
  g_object_unref(state.app);

  errands_data_cleanup();
  // errands_setting_cleanup();
  // errands_sync_cleanup();

  return status;
}
