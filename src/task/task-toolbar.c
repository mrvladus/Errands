#include "task-toolbar.h"
#include "attachments-window.h"
#include "notes-window.h"
#include "priority-window.h"
#include "tags-window.h"

G_DEFINE_TYPE(ErrandsTaskToolbar, errands_task_toolbar, GTK_TYPE_BOX)

static void errands_task_toolbar_class_init(ErrandsTaskToolbarClass *class) {}

static void errands_task_toolbar_init(ErrandsTaskToolbar *self) {
  g_object_set(self, "orientation", GTK_ORIENTATION_HORIZONTAL, "spacing", 0,
               "margin-start", 12, "margin-end", 12, "margin-bottom", 6,
               "hexpand", true, NULL);

  // Date button
  GtkWidget *date_btn_content = adw_button_content_new();
  g_object_set(date_btn_content, "icon-name", "errands-calendar-symbolic",
               "label", "Date", NULL);
  self->date_btn = gtk_button_new();
  g_object_set(self->date_btn, "child", date_btn_content, NULL);
  gtk_widget_add_css_class(self->date_btn, "flat");
  gtk_widget_add_css_class(self->date_btn, "caption");
  gtk_box_append(GTK_BOX(self), self->date_btn);

  // Notes button
  self->notes_btn = gtk_button_new_from_icon_name("errands-notes-symbolic");
  g_object_set(self->notes_btn, "halign", GTK_ALIGN_END, "hexpand", true, NULL);
  gtk_widget_add_css_class(self->notes_btn, "flat");
  gtk_box_append(GTK_BOX(self), self->notes_btn);

  // Priority button
  self->priority_btn =
      gtk_button_new_from_icon_name("errands-priority-symbolic");
  gtk_widget_add_css_class(self->priority_btn, "flat");
  gtk_box_append(GTK_BOX(self), self->priority_btn);

  // Tags button
  self->tags_btn = gtk_button_new_from_icon_name("errands-tags-symbolic");
  gtk_widget_add_css_class(self->tags_btn, "flat");
  gtk_box_append(GTK_BOX(self), self->tags_btn);

  // Attachments button
  self->attachments_btn =
      gtk_button_new_from_icon_name("errands-attachment-symbolic");
  gtk_widget_add_css_class(self->attachments_btn, "flat");
  gtk_box_append(GTK_BOX(self), self->attachments_btn);
}

ErrandsTaskToolbar *errands_task_toolbar_new(void *task) {
  ErrandsTaskToolbar *tb = g_object_new(ERRANDS_TYPE_TASK_TOOLBAR, NULL);
  g_signal_connect_swapped(tb->notes_btn, "clicked",
                           G_CALLBACK(errands_notes_window_show), task);
  g_signal_connect_swapped(tb->priority_btn, "clicked",
                           G_CALLBACK(errands_priority_window_show), task);
  g_signal_connect_swapped(tb->tags_btn, "clicked",
                           G_CALLBACK(errands_tags_window_show), task);
  g_signal_connect_swapped(tb->attachments_btn, "clicked",
                           G_CALLBACK(errands_attachments_window_show), task);
  return tb;
}
