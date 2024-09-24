#ifndef ERRANDS_UTILS_H
#define ERRANDS_UTILS_H

#include <glib.h>
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Log formatted message
#define LOG(format, ...) fprintf(stderr, "Errands: " format "\n", ##__VA_ARGS__)

// Lambda function macro
// #define lambda(lambda$_ret, lambda$_args, lambda$_body) \
//   ({ \
//     lambda$_ret lambda$__anon$ lambda$_args\
// lambda$_body &lambda$__anon$; \
//   })

// Get children of the widget
static inline GPtrArray *get_children(GtkWidget *parent) {
  GPtrArray *children = g_ptr_array_new();
  GtkWidget *first_child = gtk_widget_get_first_child(parent);
  if (first_child) {
    g_ptr_array_add(children, first_child);
    GtkWidget *next_child = gtk_widget_get_next_sibling(first_child);
    while (next_child) {
      g_ptr_array_add(children, next_child);
      next_child = gtk_widget_get_next_sibling(next_child);
    }
  }
  return children;
}

static inline char *get_date_time() {
  return g_date_time_format(g_date_time_new_now_local(), "%Y%m%dT%H%M%SZ");
}

static inline bool string_contains(const char *haystack, const char *needle) {
  return (bool)strcasestr(haystack, needle);
}

// Check if any string in the array contains string
static inline bool string_array_contains(GPtrArray *str_arr,
                                         const char *needle) {
  for (int i = 0; i < str_arr->len; i++)
    if (string_contains((char *)str_arr->pdata[i], needle))
      return true;
  return false;
}

#endif // ERRANDS_UTILS_H
