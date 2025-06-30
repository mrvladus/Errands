#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_TASK_LIST_ATTACHMENTS_DIALOG_ATTACHMENT (errands_task_list_attachment_dialog_attachment_get_type())
G_DECLARE_FINAL_TYPE(ErrandsTaskListAttachmentsDialogAttachment, errands_task_list_attachment_dialog_attachment,
                     ERRANDS, TASK_LIST_ATTACHMENTS_DIALOG_ATTACHMENT, AdwActionRow)

ErrandsTaskListAttachmentsDialogAttachment *errands_task_list_attachments_dialog_attachment_new(const char *path);
