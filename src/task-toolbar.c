#include "task-toolbar.h"
#include "data.h"
#include "notes-window.h"
#include "priority-window.h"

#include <adwaita.h>

GtkWidget *errands_task_toolbar_new(TaskData *td) {
  // Flow box
  GtkWidget *box = gtk_flow_box_new();
  // g_object_set(box, "margin-start", 12, "margin-end", 12, "margin-bottom",
  // 6, "hexpand", true, NULL);

  // Date button
  GtkWidget *date_btn_content = adw_button_content_new();
  g_object_set(date_btn_content, "icon-name", "errands-calendar-symbolic",
               "label", "Date", NULL);
  GtkWidget *date_btn = gtk_button_new();
  g_object_set(date_btn, "child", date_btn_content, NULL);
  gtk_widget_add_css_class(date_btn, "flat");
  gtk_widget_add_css_class(date_btn, "caption");
  gtk_flow_box_append(GTK_FLOW_BOX(box), date_btn);

  // Notes button
  GtkWidget *notes_btn =
      gtk_button_new_from_icon_name("errands-notes-symbolic");
  g_object_set(notes_btn, "halign", GTK_ALIGN_END, "hexpand", true, NULL);
  gtk_widget_add_css_class(notes_btn, "flat");
  g_signal_connect_swapped(notes_btn, "clicked",
                           G_CALLBACK(errands_notes_window_show), td);
  gtk_box_append(GTK_BOX(box), notes_btn);

  // Priority button
  GtkWidget *priority_btn =
      gtk_button_new_from_icon_name("errands-priority-symbolic");
  gtk_widget_add_css_class(priority_btn, "flat");
  g_signal_connect_swapped(priority_btn, "clicked",
                           G_CALLBACK(errands_priority_window_show), td);
  gtk_box_append(GTK_BOX(box), priority_btn);

  // Tags button
  // GtkWidget *tb_tags_btn =
  //     gtk_button_new_from_icon_name("errands-tags-symbolic");
  // gtk_widget_add_css_class(tb_tags_btn, "flat");
  // gtk_box_append(GTK_BOX(box), tb_tags_btn);

  // // Attachments button
  // GtkWidget *tb_attachments_btn =
  //     gtk_button_new_from_icon_name("errands-attachment-symbolic");
  // gtk_widget_add_css_class(tb_attachments_btn, "flat");
  // gtk_box_append(GTK_BOX(box), tb_attachments_btn);

  return box;
}
