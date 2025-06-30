#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_TAGS_DIALOG_TAG (errands_task_list_tags_dialog_tag_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListTagsDialogTag, errands_task_list_tags_dialog_tag, ERRANDS,
                     TASK_LIST_TAGS_DIALOG_TAG, AdwActionRow)

struct _ErrandsTaskListTagsDialogTag {
  AdwActionRow parent_instance;
  GtkWidget *check_btn;
};

ErrandsTaskListTagsDialogTag *errands_task_list_tags_dialog_tag_new(const char *tag);
