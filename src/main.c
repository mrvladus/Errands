// Copyright (c) 2026 Vlad Krupinskii <vladkrupinskii@gmail.com>
// SPDX-License-Identifier: Zlib

#include "glib-object.h"
#if !(defined(__GNUC__) || defined(__clang__))
#error "This code requires GCC or Clang compiler because it uses features not supported by other compilers.\
e.g. GLib's g_autoptr, g_auto and g_autofree"
#endif

#include "config.h"
#include "data.h"
#include "settings.h"
#include "sidebar.h"
#include "state.h"
#include "sync.h"
#include "utils.h"
#include "window.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <libportal-gtk4/portal-gtk4.h>

extern GResource *errands_get_resource(void);
State state = {0};

static void activate(GtkApplication *app) {
  state.main_window = errands_window_new(app);
  gtk_window_present(GTK_WINDOW(state.main_window));
  errands_sidebar_select_last_opened_page();
  errands_sync();

  // Request background
  // g_autoptr(XdpPortal) portal = xdp_portal_new();
  // g_autoptr(XdpParent) parent = xdp_parent_new_gtk(GTK_WINDOW(state.main_window));
  // if (errands_settings_get_startup(state.settings)) {
  //   g_autoptr(GPtrArray) cmdline = g_ptr_array_sized_new(2);
  //   g_ptr_array_add(cmdline, "errands");
  //   g_ptr_array_add(cmdline, "--gapplication-service");
  //   xdp_portal_request_background(portal, parent, "Errands needs to run in the background for sending notifications", cmdline,
  //                                 XDP_BACKGROUND_FLAG_AUTOSTART, NULL, NULL, NULL);
  // } else xdp_portal_request_background(portal, NULL, NULL, NULL, XDP_BACKGROUND_FLAG_NONE, NULL, NULL, NULL);
}

int main(int argc, char **argv) {
  RANDOM_SEED();
  g_message("Starting version %s (%s) %sFlatpak", VERSION, VERSION_COMMIT, xdp_portal_running_under_flatpak() ? "" : "not ");

  // Setup locales
  bindtextdomain("errands", LOCALE_DIR);
  bind_textdomain_codeset("errands", "UTF-8");
  textdomain("errands");

  // Initialize systems
  state.settings = errands_settings_new();
  errands_sync_init();
  errands_data_init();

  // Run application
  state.app = adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(state.app, "activate", G_CALLBACK(activate), NULL);
  g_resources_register(errands_get_resource());
  g_application_set_resource_base_path(G_APPLICATION(state.app), RESOURCE_PATH);
  const int status = g_application_run(G_APPLICATION(state.app), argc, argv);

  // Cleanup
  errands_sync_cleanup();
  errands_data_cleanup();
  g_clear_object(&state.settings);
  g_clear_object(&state.app);

  return status;
}
