#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_SETTINGS_DIALOG (errands_settings_dialog_get_type())
G_DECLARE_FINAL_TYPE(ErrandsSettingsDialog, errands_settings_dialog, ERRANDS, SETTINGS_DIALOG, AdwWindow)

ErrandsSettingsDialog *errands_settings_dialog_new();
void errands_settings_dialog_show();
