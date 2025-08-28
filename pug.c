#define PUG_IMPLEMENTATION
#include "pug.h"

#define APP_ID     "io.github.mrvladus.Errands"
#define VERSION    "48.0"
#define LOCALE_DIR ""
#define BUILD_DIR  "build"

static void compile_resources() {
  const char *gresource_xml = "data/errands.gresource.xml";
  const char *resoure_c = "src/resources.c";
  if (pug_file_changed_after(gresource_xml, resoure_c)) {
    pug_log("'%s' is changed. Compiling resources.", gresource_xml);
    if (!pug_cmd("blueprint-compiler batch-compile %s src src/*.blp", BUILD_DIR)) exit(1);
    if (!pug_cmd("glib-compile-resources --generate-source --target=%s --c-name=errands %s", resoure_c, gresource_xml))
      exit(1);
  }
}

static bool build_errands() {
  PugTarget errands = pug_target_new("errands", PUG_TARGET_TYPE_EXECUTABLE, "build");
  pug_target_add_sources(
      &errands, "src/data/data.c", "src/data/data-io.c", "src/data/data-props.c", "src/sync/sync.c",
      "src/sync/caldav/caldav-calendar.c", "src/sync/caldav/caldav-client.c", "src/sync/caldav/caldav-event.c",
      "src/sync/caldav/caldav-list.c", "src/sync/caldav/caldav-requests.c", "src/main.c", "src/resources.c",
      "src/settings.c", "src/sidebar.c", "src/sidebar-all-row.c", "src/sidebar-delete-list-dialog.c",
      "src/sidebar-new-list-dialog.c", "src/sidebar-rename-list-dialog.c", "src/sidebar-task-list-row.c", "src/state.c",
      "src/task.c", "src/task-list.c", "src/task-list-attachments-dialog.c",
      "src/task-list-attachments-dialog-attachment.c", "src/task-list-color-dialog.c", "src/task-list-date-dialog.c",
      "src/task-list-date-dialog-date-chooser.c", "src/task-list-date-dialog-time-chooser.c",
      "src/task-list-date-dialog-rrule-row.c", "src/task-list-notes-dialog.c", "src/task-list-priority-dialog.c",
      "src/task-list-sort-dialog.c", "src/task-list-tags-dialog.c", "src/task-list-tags-dialog-tag.c", "src/window.c",
      NULL);

  pug_target_add_cflags(&errands, CFLAG_DEFINE_STR(APP_ID), CFLAG_DEFINE_STR(VERSION), CFLAG_DEFINE_STR(LOCALE_DIR),
                        NULL);
  pug_target_add_pkg_config_libs(&errands, "libadwaita-1", "libcurl", "libical", "gtksourceview-5", "webkitgtk-6.0",
                                 "libportal", NULL);

  return pug_target_build(&errands);
}

int main(int argc, char **argv) {
  pug_init(argc, argv);

  // Remove build directory if `clean` argument provided
  if (pug_arg_bool("clean")) {
    pug_cmd("rm -rf " BUILD_DIR);
    return 0;
  }

  compile_resources();
  bool res = build_errands();
  // Run if `run` argument provided
  if (res && pug_arg_bool("run")) pug_cmd(BUILD_DIR "/errands");

  return 0;
}
