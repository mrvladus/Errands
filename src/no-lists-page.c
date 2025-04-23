#include "no-lists-page.h"
#include "state.h"

#include <glib/gi18n.h>

static void on_button_clicked() { g_signal_emit_by_name(state.sidebar->add_btn, "activate", NULL); }

G_DEFINE_TYPE(ErrandsNoListsPage, errands_no_lists_page, ADW_TYPE_BIN)

static void errands_no_lists_page_class_init(ErrandsNoListsPageClass *class) {}

static void errands_no_lists_page_init(ErrandsNoListsPage *self) {
  gtk_widget_add_css_class(GTK_WIDGET(self), "background");

  GtkWidget *btn = g_object_new(GTK_TYPE_BUTTON, "icon-name", "errands-add-symbolic", "halign", GTK_ALIGN_CENTER,
                                "css-classes", (const char *[]){"pill", "suggested-action", NULL}, NULL);
  g_signal_connect(btn, "clicked", G_CALLBACK(on_button_clicked), NULL);

  GtkWidget *sp = g_object_new(ADW_TYPE_STATUS_PAGE, "title", _("Add Task List"), "child", btn, "icon-name",
                               "io.github.mrvladus.List", NULL);

  GtkWidget *tb = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(tb), g_object_new(ADW_TYPE_HEADER_BAR, "show-title", false, NULL));
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(tb), sp);
  adw_bin_set_child(ADW_BIN(self), tb);
}

ErrandsNoListsPage *errands_no_lists_page_new() { return g_object_new(ERRANDS_TYPE_NO_LISTS_PAGE, NULL); }
