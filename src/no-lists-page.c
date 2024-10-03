#include "no-lists-page.h"

G_DEFINE_TYPE(ErrandsNoListsPage, errands_no_lists_page, ADW_TYPE_BIN)

static void errands_no_lists_page_class_init(ErrandsNoListsPageClass *class) {}

static void errands_no_lists_page_init(ErrandsNoListsPage *self) {}

ErrandsNoListsPage *errands_no_lists_page_new() {
  return g_object_new(ERRANDS_TYPE_NO_LISTS_PAGE, NULL);
}
