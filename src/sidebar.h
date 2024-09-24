#ifndef ERRANDS_SIDEBAR_H
#define ERRANDS_SIDEBAR_H

#include <adwaita.h>

void errands_sidebar_build();
GtkWidget *errands_sidebar_get_list_row(const char *uid);

#endif // ERRANDS_SIDEBAR_H
