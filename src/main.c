#include "config.h"
#include "data.h"
#include "resources.h"
#include "settings.h"
#include "state.h"
// #include "sync.h"
#include "window.h"

#include <adwaita.h>
#include <glib/gi18n.h>

#include <stddef.h>

static void activate(GtkApplication *app) {
  state.main_window = errands_window_new();
  errands_window_build(state.main_window);
  gtk_window_present(GTK_WINDOW(state.main_window));
  // sync_init();
}

int main(int argc, char **argv) {
  // Generate random seed
  srand((unsigned int)(time(NULL) ^ getpid()));

  bindtextdomain("errands", LOCALE_DIR);
  bind_textdomain_codeset("errands", "UTF-8");
  textdomain("errands");

  state.tl_data = errands_data_load_lists();
  state.t_data = errands_data_load_tasks(state.tl_data);

  GPtrArray *l = errands_data_load_lists();
  GPtrArray *t = errands_data_load_tasks(l);

  errands_settings_init();

  state.app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(state.app, "activate", G_CALLBACK(activate), NULL);
  g_resources_register(errands_get_resource());
  int status = g_application_run(G_APPLICATION(state.app), argc, argv);
  g_object_unref(state.app);
  return status;
}
