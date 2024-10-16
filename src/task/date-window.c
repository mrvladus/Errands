#include "date-window.h"
#include "../state.h"
#include "../utils.h"
#include "adwaita.h"

#include <glib/gi18n.h>

static void on_errands_date_window_close_cb(ErrandsDateWindow *win,
                                            gpointer data);

G_DEFINE_TYPE(ErrandsDateWindow, errands_date_window, ADW_TYPE_DIALOG)

static void errands_date_window_class_init(ErrandsDateWindowClass *class) {}

static void errands_date_window_init(ErrandsDateWindow *self) {
  LOG("Creating date window");

  g_object_set(self, "title", _("Date and Time"), "content-width", 360,
               "content-height", 400, NULL);
  g_signal_connect(self, "closed", G_CALLBACK(on_errands_date_window_close_cb),
                   NULL);

  // Scrolled window
  GtkWidget *scrl = gtk_scrolled_window_new();
  g_object_set(scrl, "propagate-natural-height", true, NULL);
  // gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrl), self->view);

  // Toolbar view
  GtkWidget *tb = adw_toolbar_view_new();
  g_object_set(tb, "content", scrl, NULL);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), adw_header_bar_new());
  adw_dialog_set_child(ADW_DIALOG(self), tb);
}

ErrandsDateWindow *errands_date_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_DATE_WINDOW, NULL));
}

void errands_date_window_show(ErrandsTask *task) {
  state.date_window->task = task;
  adw_dialog_present(ADW_DIALOG(state.date_window),
                     GTK_WIDGET(state.main_window));
}

// --- SIGNAL HANDLERS --- //

static void on_errands_date_window_close_cb(ErrandsDateWindow *win,
                                            gpointer data) {}
