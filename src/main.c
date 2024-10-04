#include "data.h"
#include "resources.h"
#include "state.h"
#include "window.h"

#include <adwaita.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void activate(GtkApplication *app) {
  errands_window_build();
  gtk_window_present(GTK_WINDOW(state.main_window));
}

int main(int argc, char **argv) {
  errands_data_load();
  state.app = adw_application_new("io.github.mrvladus.Errands",
                                  G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(state.app, "activate", G_CALLBACK(activate), NULL);
  g_resources_register(errands_get_resource());
  int status = g_application_run(G_APPLICATION(state.app), argc, argv);
  g_object_unref(state.app);
  return status;
}
