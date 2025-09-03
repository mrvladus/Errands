#include "caldav-utils.h"
#include "caldav.h"

CalDAVList *caldav_list_new() {
  CalDAVList *list = (CalDAVList *)malloc(sizeof(CalDAVList)); // Correct allocation
  if (list == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }
  list->len = 0;
  list->size = 1;
  list->data = (void **)malloc(list->size * sizeof(void *));
  if (list->data == NULL) {
    fprintf(stderr, "Memory allocation for data array failed\n");
    caldav_free(list);
    return NULL;
  }
  return list;
}

void caldav_list_add(CalDAVList *list, void *data) {
  if (list->len >= list->size) {
    list->size *= 2;
    void **new_data = (void **)realloc(list->data, list->size * sizeof(void *));
    if (new_data == NULL) {
      fprintf(stderr, "Memory reallocation failed\n");
      return;
    }
    list->data = new_data;
  }
  list->data[list->len++] = data;
}

void *caldav_list_at(CalDAVList *list, size_t index) {
  if (index >= list->len)
    return NULL;
  return list->data[index];
}

void caldav_list_free(CalDAVList *list, void (*data_free_func)(void *)) {
  if (list != NULL) {
    if (data_free_func != NULL)
      for (size_t i = 0; i < list->len; i++)
        data_free_func(list->data[i]);
    caldav_free(list->data);
    caldav_free(list);
  }
}
