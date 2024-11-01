#include "task-toolbar.h"
#include "attachments-window.h"
#include "color-window.h"
#include "date-window.h"
#include "notes-window.h"
#include "priority-window.h"
#include "tags-window.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE(ErrandsTaskToolbar, errands_task_toolbar, ADW_TYPE_BIN)

static void errands_task_toolbar_class_init(ErrandsTaskToolbarClass *class) {}

static void errands_task_toolbar_init(ErrandsTaskToolbar *self) {
  GtkWidget *box = gtk_flow_box_new();
  g_object_set(box, "selection-mode", GTK_SELECTION_NONE, "margin-start", 12, "margin-end", 12,
               "margin-bottom", 6, "max-children-per-line", 2, NULL);
  adw_bin_set_child(ADW_BIN(self), box);

  // Date button
  GtkWidget *date_btn_content = adw_button_content_new();
  g_object_set(date_btn_content, "icon-name", "errands-calendar-symbolic", "label", _("Date"),
               NULL);
  self->date_btn = gtk_button_new();
  g_object_set(self->date_btn, "child", date_btn_content, "halign", GTK_ALIGN_START, "hexpand",
               true, NULL);
  gtk_widget_add_css_class(self->date_btn, "flat");
  gtk_widget_add_css_class(self->date_btn, "caption");
  gtk_flow_box_append(GTK_FLOW_BOX(box), self->date_btn);

  // Priority button
  self->priority_btn = gtk_button_new_from_icon_name("errands-priority-symbolic");
  gtk_widget_add_css_class(self->priority_btn, "flat");

  // Notes button
  self->notes_btn = gtk_button_new_from_icon_name("errands-notes-symbolic");
  gtk_widget_add_css_class(self->notes_btn, "flat");

  // Tags button
  self->tags_btn = gtk_button_new_from_icon_name("errands-tags-symbolic");
  gtk_widget_add_css_class(self->tags_btn, "flat");

  // Attachments button
  self->attachments_btn = gtk_button_new_from_icon_name("errands-attachment-symbolic");
  gtk_widget_add_css_class(self->attachments_btn, "flat");

  // Color button
  self->color_btn = gtk_button_new_from_icon_name("errands-color-symbolic");
  gtk_widget_add_css_class(self->color_btn, "flat");

  // End box
  GtkWidget *ebox = g_object_new(GTK_TYPE_BOX, "halign", GTK_ALIGN_END, NULL);
  gtk_box_append(GTK_BOX(ebox), self->priority_btn);
  gtk_box_append(GTK_BOX(ebox), self->notes_btn);
  gtk_box_append(GTK_BOX(ebox), self->tags_btn);
  gtk_box_append(GTK_BOX(ebox), self->attachments_btn);
  gtk_box_append(GTK_BOX(ebox), self->color_btn);
  gtk_flow_box_append(GTK_FLOW_BOX(box), ebox);
}

ErrandsTaskToolbar *errands_task_toolbar_new(void *task) {
  ErrandsTaskToolbar *tb = g_object_new(ERRANDS_TYPE_TASK_TOOLBAR, NULL);
  // Connect signals
  g_signal_connect_swapped(tb->date_btn, "clicked", G_CALLBACK(errands_date_window_show), task);
  g_signal_connect_swapped(tb->notes_btn, "clicked", G_CALLBACK(errands_notes_window_show), task);
  g_signal_connect_swapped(tb->priority_btn, "clicked", G_CALLBACK(errands_priority_window_show),
                           task);
  g_signal_connect_swapped(tb->tags_btn, "clicked", G_CALLBACK(errands_tags_window_show), task);
  g_signal_connect_swapped(tb->attachments_btn, "clicked",
                           G_CALLBACK(errands_attachments_window_show), task);
  g_signal_connect_swapped(tb->color_btn, "clicked", G_CALLBACK(errands_color_window_show), task);
  // Update css for buttons
  ErrandsTask *_task = task;
  // Notes button
  if (strcmp(_task->data->notes, ""))
    gtk_widget_add_css_class(tb->notes_btn, "accent");
  // Attachments button
  if (_task->data->attachments->len > 0)
    gtk_widget_add_css_class(tb->attachments_btn, "accent");
  // Priority button
  int priority = _task->data->priority;
  gtk_button_set_icon_name(GTK_BUTTON(tb->priority_btn), priority > 0
                                                             ? "errands-priority-set-symbolic"
                                                             : "errands-priority-symbolic");
  if (priority > 0 && priority < 3)
    gtk_widget_add_css_class(tb->priority_btn, "priority-low");
  else if (priority > 3 && priority < 6)
    gtk_widget_add_css_class(tb->priority_btn, "priority-medium");
  else if (priority > 6 && priority < 10)
    gtk_widget_add_css_class(tb->priority_btn, "priority-high");
  return tb;
}
