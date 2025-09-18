#include <stdbool.h>
#define PUG_IMPLEMENTATION
#include "pug.h"

#define BIN_NAME   "errands"
#define APP_ID     "io.github.mrvladus.Errands"
#define VERSION    "49.0"
#define LOCALE_DIR ""

#define BUILD_DIR "build"

static bool is_debug = false;

// Errands caldav backend
static bool build_libcaldav() {
  PugTarget libcaldav = pug_target_new("libcaldav", PUG_TARGET_TYPE_STATIC_LIBRARY, BUILD_DIR);

  // Sources
  pug_target_add_source(&libcaldav, "src/libcaldav/caldav-calendar.c");
  pug_target_add_source(&libcaldav, "src/libcaldav/caldav-client.c");
  pug_target_add_source(&libcaldav, "src/libcaldav/caldav-event.c");
  pug_target_add_source(&libcaldav, "src/libcaldav/caldav-list.c");
  pug_target_add_source(&libcaldav, "src/libcaldav/caldav-requests.c");

  // CFLAGS
  pug_target_add_cflags(&libcaldav, "-Wall");
  if (!is_debug) pug_target_add_cflags(&libcaldav, "-O3", "-flto=auto");
  else pug_target_add_cflag(&libcaldav, "-g");

  // LDFLAGS
  if (!is_debug) pug_target_add_ldflags(&libcaldav, "-O3", "-flto=auto");

  // pkg-config libraries
  pug_target_add_pkg_config_lib(&libcaldav, "libcurl");
  pug_target_add_pkg_config_lib(&libcaldav, "libical");

  return pug_target_build(&libcaldav);
}

static bool compile_resources() {
  const char *gresource_xml = "data/errands.gresource.xml";
  const char *resoures_c = "src/resources.c";

  bool styles_changed =
      pug_file_is_older_than_files(resoures_c, "data/styles/style.css", "data/styles/style-dark.css", NULL);

  bool blp_files_changed = pug_file_is_older_than_files(
      resoures_c, "src/sidebar.blp", "src/sidebar-all-row.blp", "src/sidebar-delete-list-dialog.blp",
      "src/sidebar-new-list-dialog.blp", "src/sidebar-rename-list-dialog.blp", "src/sidebar-task-list-row.blp",
      "src/task.blp", "src/task-list.blp", "src/task-list-attachments-dialog.blp",
      "src/task-list-attachments-dialog-attachment.blp", "src/task-list-color-dialog.blp",
      "src/task-list-date-dialog.blp", "src/task-list-date-dialog-date-chooser.blp",
      "src/task-list-date-dialog-rrule-row.blp", "src/task-list-date-dialog-time-chooser.blp",
      "src/task-list-notes-dialog.blp", "src/task-list-priority-dialog.blp", "src/task-list-sort-dialog.blp",
      "src/task-list-tags-dialog.blp", "src/task-list-tags-dialog-tag.blp", "src/window.blp", NULL);

  bool gresource_xml_changed = pug_file1_is_older_than_file2(resoures_c, gresource_xml);

  if (styles_changed || blp_files_changed || gresource_xml_changed) {
    pug_log("Compiling resources.");
    if (!pug_cmd("blueprint-compiler batch-compile %s src src/*.blp", BUILD_DIR)) return false;
    if (!pug_cmd("glib-compile-resources --generate-source --target=%s --c-name=errands %s", resoures_c, gresource_xml))
      return false;
  }

  return true;
}

static bool build_errands() {
  PugTarget errands = pug_target_new(BIN_NAME, PUG_TARGET_TYPE_EXECUTABLE, BUILD_DIR);

  // Sources
  pug_target_add_source(&errands, "src/data/data.c");
  pug_target_add_source(&errands, "src/data/data-io.c");
  pug_target_add_source(&errands, "src/data/data-props.c");
  pug_target_add_source(&errands, "src/sync.c");
  pug_target_add_source(&errands, "src/main.c");
  pug_target_add_source(&errands, "src/resources.c");
  pug_target_add_source(&errands, "src/settings.c");
  pug_target_add_source(&errands, "src/sidebar.c");
  pug_target_add_source(&errands, "src/sidebar-all-row.c");
  pug_target_add_source(&errands, "src/sidebar-delete-list-dialog.c");
  pug_target_add_source(&errands, "src/sidebar-new-list-dialog.c");
  pug_target_add_source(&errands, "src/sidebar-rename-list-dialog.c");
  pug_target_add_source(&errands, "src/sidebar-task-list-row.c");
  pug_target_add_source(&errands, "src/state.c");
  pug_target_add_source(&errands, "src/task.c");
  pug_target_add_source(&errands, "src/task-list.c");
  pug_target_add_source(&errands, "src/task-list-attachments-dialog.c");
  pug_target_add_source(&errands, "src/task-list-attachments-dialog-attachment.c");
  pug_target_add_source(&errands, "src/task-list-color-dialog.c");
  pug_target_add_source(&errands, "src/task-list-date-dialog.c");
  pug_target_add_source(&errands, "src/task-list-date-dialog-date-chooser.c");
  pug_target_add_source(&errands, "src/task-list-date-dialog-time-chooser.c");
  pug_target_add_source(&errands, "src/task-list-date-dialog-rrule-row.c");
  pug_target_add_source(&errands, "src/task-list-notes-dialog.c");
  pug_target_add_source(&errands, "src/task-list-priority-dialog.c");
  pug_target_add_source(&errands, "src/task-list-sort-dialog.c");
  pug_target_add_source(&errands, "src/task-list-tags-dialog.c");
  pug_target_add_source(&errands, "src/task-list-tags-dialog-tag.c");
  pug_target_add_source(&errands, "src/window.c");

  // CFLAGS
  pug_target_add_cflags(&errands, "-Wall");
  pug_target_add_cflags(&errands, "-Wno-unknown-pragmas", "-Wno-enum-int-mismatch");
  pug_target_add_cflag(&errands, PUG_CFLAG_DEFINE_STR(APP_ID));
  pug_target_add_cflag(&errands, PUG_CFLAG_DEFINE_STR(VERSION));
  pug_target_add_cflag(&errands, PUG_CFLAG_DEFINE_STR(LOCALE_DIR));
  if (!is_debug) pug_target_add_cflags(&errands, "-O3", "-flto=auto");
  else pug_target_add_cflag(&errands, "-g");

  // LDFLAGS
  pug_target_add_ldflags(&errands, PUG_LDFLAG_LIB_DIR(BUILD_DIR), PUG_LDFLAG_LIB("caldav"));
  if (!is_debug) pug_target_add_ldflags(&errands, "-O3", "-flto=auto");

  // pkg-config libraries
  pug_target_add_pkg_config_lib(&errands, "gtksourceview-5");
  pug_target_add_pkg_config_lib(&errands, "libadwaita-1");
  pug_target_add_pkg_config_lib(&errands, "libical");
  pug_target_add_pkg_config_lib(&errands, "libportal");
  pug_target_add_pkg_config_lib(&errands, "libcurl");
  pug_target_add_pkg_config_lib(&errands, "webkitgtk-6.0");

  return pug_target_build(&errands);
}

bool install() { return true; }

int main(int argc, char **argv) {
  pug_init(argc, argv);

  // Remove build directory if `clean` argument provided
  if (pug_arg_bool("clean")) {
    pug_cmd("rm -rf " BUILD_DIR);
    return 0;
  }

  // Check if build is in debug mode
  is_debug = pug_arg_bool("debug");

  if (!build_libcaldav()) return 1;
  if (!compile_resources()) return 1;
  if (!build_errands()) return 1;

  // Install if `install` argument provided
  if (pug_arg_bool("install")) return install();

  // Run if `run` argument provided
  if (pug_arg_bool("run")) pug_cmd(BUILD_DIR "/" BIN_NAME);

  return 0;
}
