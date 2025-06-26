#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_WIDGET (errands_widget_get_type())
G_DECLARE_FINAL_TYPE(ErrandsWidget, errands_widget, ERRANDS, WIDGET, AdwBin)

struct _ErrandsWidget {
  AdwBin parent_instance;
  GtkWidget *child;
};

ErrandsWidget *errands_widget_new();
