#include "date-window.h"
#include "../state.h"

#include <glib/gi18n.h>

static void on_errands_date_window_close_cb(ErrandsDateWindow *win,
                                            gpointer data);

G_DEFINE_TYPE(ErrandsDateWindow, errands_date_window, ADW_TYPE_DIALOG)

static void errands_date_window_class_init(ErrandsDateWindowClass *class) {}

static void errands_date_window_init(ErrandsDateWindow *self) {}

ErrandsDateWindow *errands_date_window_new() {
  return g_object_ref_sink(g_object_new(ERRANDS_TYPE_DATE_WINDOW, NULL));
}

void errands_notes_window_show(ErrandsTask *task) {
  state.date_window->task = task;
  adw_dialog_present(ADW_DIALOG(state.date_window),
                     GTK_WIDGET(state.main_window));
}
