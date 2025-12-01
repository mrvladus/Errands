#include "about-dialog.h"
#include "config.h"
#include "state.h"

#include <adwaita.h>
#include <glib/gi18n.h>

static const char *release_notes = "<p>This is release notes.</p>";

void errands_about_dialog_show() {
  const char *developers[] = {"Vlad Krupinskii", NULL};

  adw_show_about_dialog(
      GTK_WIDGET(state.main_window), "application-name", _("Errands"), "application-icon", APP_ID, "version", VERSION,
      "copyright", "© 2025 Vlad Krupinskii", "website", "https://github.com/mrvladus/Errands", "issue-url",
      "https://github.com/mrvladus/Errands/issues/", "license-type", GTK_LICENSE_MIT_X11, "developers", developers,
      "translator-credits", _("translator-credits"), "release-notes", release_notes, NULL);
}
