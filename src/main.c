#if !(defined(__GNUC__) || defined(__clang__))
#error "This code requires GCC or Clang compiler because it uses features not supported by other compilers.\
e.g. GLib's g_autoptr, g_auto and g_autofree"
#endif

#ifndef APP_ID
#define APP_ID ""
#endif
#ifndef VERSION
#define VERSION ""
#endif
#ifndef LOCALE_DIR
#define LOCALE_DIR ""
#endif

#include "data/data.h"
#include "settings.h"
#include "state.h"
#include "widgets.h"

#define TOOLBOX_IMPLEMENTATION
#include "vendor/toolbox.h"

#include <glib/gi18n.h>
#include <libportal/portal.h>

extern GResource *errands_get_resource(void);

static void activate(GtkApplication *app) {
  state.main_window = errands_window_new(app);
  errands_sidebar_load_lists(ERRANDS_SIDEBAR(state.main_window->sidebar));
  errands_task_list_load_tasks(state.main_window->task_list);
  gtk_window_present(GTK_WINDOW(state.main_window));
  // sync_init();
}

int main(int argc, char **argv) {
  tb_log_set_prefix("\033[0;32m[Errands] \033[0m");
  tb_log("Starting Errands %s %s", VERSION, xdp_portal_running_under_flatpak() ? "(flatpak)" : "(not flatpak)");
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
