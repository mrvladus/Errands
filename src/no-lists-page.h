#pragma once

#include <adwaita.h>

#define ERRANDS_TYPE_NO_LISTS_PAGE (errands_no_lists_page_get_type())
G_DECLARE_FINAL_TYPE(ErrandsNoListsPage, errands_no_lists_page, ERRANDS,
                     NO_LISTS_PAGE, AdwBin)

struct _ErrandsNoListsPage {
  AdwBin parent_instance;
};

ErrandsNoListsPage *errands_no_lists_page_new();
