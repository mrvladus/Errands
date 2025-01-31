#include "task-toolbar.h"
#include "data.h"
#include "notes-window.h"
#include "priority-window.h"
#include "task.h"
#include "utils.h"
// #include "attachments-window.h"
// #include "color-window.h"
// #include "date-window.h"
// #include "tags-window.h"

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

ErrandsTaskToolbar *errands_task_toolbar_new(ErrandsTask *task) {
  ErrandsTaskToolbar *tb = g_object_new(ERRANDS_TYPE_TASK_TOOLBAR, NULL);
  tb->task = task;
  // Connect signals
  // g_signal_connect_swapped(tb->date_btn, "clicked", G_CALLBACK(errands_date_window_show), task);
  g_signal_connect_swapped(tb->notes_btn, "clicked", G_CALLBACK(errands_notes_window_show), task);
  g_signal_connect_swapped(tb->priority_btn, "clicked", G_CALLBACK(errands_priority_window_show),
                           task);
  // g_signal_connect_swapped(tb->tags_btn, "clicked", G_CALLBACK(errands_tags_window_show), task);
  // g_signal_connect_swapped(tb->attachments_btn, "clicked",
  //                          G_CALLBACK(errands_attachments_window_show), task);
  // g_signal_connect_swapped(tb->color_btn, "clicked", G_CALLBACK(errands_color_window_show),
  // task); Update css for buttons
  // Notes button
  if (strcmp(task_data_get_notes(task->data), ""))
    gtk_widget_add_css_class(tb->notes_btn, "accent");
  // Attachments button
  // if (task_data_get_attachments(task->data)->len > 0)
  //   gtk_widget_add_css_class(tb->attachments_btn, "accent");
  // Priority button
  uint8_t priority = task_data_get_priority(task->data);
  gtk_button_set_icon_name(GTK_BUTTON(tb->priority_btn), priority > 0
                                                             ? "errands-priority-set-symbolic"
                                                             : "errands-priority-symbolic");
  if (priority > 0 && priority < 3)
    gtk_widget_add_css_class(tb->priority_btn, "priority-low");
  else if (priority > 3 && priority < 6)
    gtk_widget_add_css_class(tb->priority_btn, "priority-medium");
  else if (priority > 6 && priority < 10)
    gtk_widget_add_css_class(tb->priority_btn, "priority-high");

  // Update buttons
  errands_task_toolbar_update_date_btn(tb);

  return tb;
}

void errands_task_toolbar_update_date_btn(ErrandsTaskToolbar *tb) {
  TaskData *data = tb->task->data;
  // If not repeated
  if (!strcmp(task_data_get_rrule(data), "")) {
    // If no due date - set "Date" label
    const char *due = task_data_get_due(data);
    if (!strcmp(due, "")) {
      adw_button_content_set_label(
          ADW_BUTTON_CONTENT(gtk_button_get_child(GTK_BUTTON(tb->date_btn))), _("Date"));
    }
    // If due date is set
    else {
      g_autoptr(GDateTime) dt = NULL;
      g_autofree gchar *label = NULL;
      if (!string_contains(due, "T")) {
        char new_dt[16];
        sprintf(new_dt, "%sT000000Z", due);
        dt = g_date_time_new_from_iso8601(new_dt, NULL);
        label = g_date_time_format(dt, "%d %b");
      } else {
        dt = g_date_time_new_from_iso8601(due, NULL);
        label = g_date_time_format(dt, "%d %b %R");
      }
      adw_button_content_set_label(
          ADW_BUTTON_CONTENT(gtk_button_get_child(GTK_BUTTON(tb->date_btn))), label);
    }
  }
  // If repeated
  // else {
  //   str label = str_new("");

  //   // Get interval
  //   char *inter = get_rrule_value(data->rrule, "INTERVAL");
  //   int interval;
  //   if (inter) {
  //     interval = atoi(inter);
  //     free(inter);
  //   } else
  //     interval = 1;

  //   // Get frequency
  //   char *frequency = get_rrule_value(data->rrule, "FREQ");
  //   if (!strcmp(frequency, "MINUTELY"))
  //     interval == 1 ? str_append(&label, _("Every minute"))
  //                   : str_append_printf(&label, _("Every %d minutes"), interval);
  //   else if (!strcmp(frequency, "HOURLY"))
  //     interval == 1 ? str_append(&label, _("Every hour"))
  //                   : str_append_printf(&label, _("Every %d hours"), interval);
  //   else if (!strcmp(frequency, "DAILY"))
  //     interval == 1 ? str_append(&label, _("Every day"))
  //                   : str_append_printf(&label, _("Every %d days"), interval);
  //   else if (!strcmp(frequency, "WEEKLY"))
  //     interval == 1 ? str_append(&label, _("Every week"))
  //                   : str_append_printf(&label, _("Every %d weeks"), interval);
  //   else if (!strcmp(frequency, "MONTHLY"))
  //     interval == 1 ? str_append(&label, _("Every month"))
  //                   : str_append_printf(&label, _("Every %d months"), interval);
  //   else if (!strcmp(frequency, "YEARLY"))
  //     interval == 1 ? str_append(&label, _("Every year"))
  //                   : str_append_printf(&label, _("Every %d years"), interval);
  //   adw_button_content_set_label(ADW_BUTTON_CONTENT(gtk_button_get_child(GTK_BUTTON(tb->date_btn))),
  //                                label.str);
  //   free(frequency);
  //   str_free(&label);
  // }
}
