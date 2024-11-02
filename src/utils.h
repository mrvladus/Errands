#pragma once

#include <ctype.h>
#include <gtk/gtk.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// --- MACROS --- //

// Log formatted message
#define LOG(format, ...) fprintf(stderr, "Errands: " format "\n", ##__VA_ARGS__)

// For range
#define for_range(var, from, to) for (int var = from; var < to; var++)

// Avoid dangling pointers
#define free(mem)                                                                                  \
  free(mem);                                                                                       \
  mem = NULL;

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
static inline bool string_array_contains(GPtrArray *str_arr, const char *needle) {
  for (int i = 0; i < str_arr->len; i++)
    if (string_contains((char *)str_arr->pdata[i], needle))
      return true;
  return false;
}

// Function to trim leading and trailing whitespace
static inline char *string_trim(char *str) {
  // Trim leading whitespace
  while (isspace((unsigned char)*str))
    str++;
  // Trim trailing whitespace
  char *end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;
  // Null terminate the trimmed string
  *(end + 1) = '\0';
  return str;
}

static inline bool file_exists(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file) {
    fclose(file);
    return true;
  }
  return false;
}

static inline bool directory_exists(const char *path) {
  struct stat statbuf;
  return (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
}

static inline char *generate_hex() {
  char *color = (char *)malloc(8 * sizeof(char));
  int red = rand() % 256;
  int green = rand() % 256;
  int blue = rand() % 256;
  sprintf(color, "#%02X%02X%02X", red, green, blue);
  return color;
}

static inline char *gdk_rgba_to_hex_string(const GdkRGBA *rgba) {
  // Allocate memory for the hex string (7 characters: #RRGGBB + null
  // terminator)
  char *hex_string = malloc(8);
  // Convert the RGBA components to integers in the range [0, 255]
  int r = (int)(rgba->red * 255);
  int g = (int)(rgba->green * 255);
  int b = (int)(rgba->blue * 255);
  // Format the string as #RRGGBB
  snprintf(hex_string, 8, "#%02X%02X%02X", r, g, b);
  return hex_string;
}

// Add shortcut to the widget
static inline void errands_add_shortcut(GtkWidget *widget, const char *trigger,
                                        const char *action) {
  GtkEventController *ctrl = gtk_shortcut_controller_new();
  gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(ctrl), GTK_SHORTCUT_SCOPE_GLOBAL);
  GtkShortcut *sc = gtk_shortcut_new(gtk_shortcut_trigger_parse_string(trigger),
                                     gtk_shortcut_action_parse_string(action));
  gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ctrl), sc);
  gtk_widget_add_controller(widget, ctrl);
}

static inline char *get_rrule_value(const char *rrule, const char *key) {
  const char *value_start = strstr(rrule, key);
  if (!value_start)
    return NULL;
  value_start += strlen(key) + 1;
  const char *value_end = strchr(value_start, ';');
  if (!value_end)
    return NULL;
  int length = value_end - value_start;
  char *out = malloc(length);
  out[length] = '\0';
  strncpy(out, value_start, length);
  return out;
}

static bool string_is_number(const char *str) {
  // Check for empty string
  if (str == NULL || *str == '\0')
    return false; // Not a number

  char *endptr;
  // Try to convert to long
  strtol(str, &endptr, 10);
  if (endptr != str && *endptr == '\0') {
    return true; // Valid integer
  }

  // Try to convert to double
  strtod(str, &endptr);
  if (endptr != str && *endptr == '\0') {
    return true; // Valid floating-point number
  }

  return false;
}

static inline int *string_to_int_array(const char *str) {
  // First, count the number of integers
  int count = 1;
  for (const char *p = str; *p != '\0'; p++) {
    if (*p == ',')
      count++;
  }

  // Allocate memory for the array, with one extra space for NULL termination
  int *arr = (int *)malloc((count + 1) * sizeof(int));
  if (!arr)
    return NULL;

  // Parse the string and fill the array
  int index = 0;
  const char *start = str;
  char *end;
  while (*start != '\0') {
    arr[index++] = strtol(start, &end, 10);
    start = end + 1;
  }

  arr[index] = 0; // NULL terminate the array
  return arr;
}

// ---------- DYNAMIC STRING ---------- //

// Struct to hold string data
typedef struct {
  char *str;
  int len;
} str;

// Creates new string
inline str str_new(const char *init_str) {
  str s;
  s.len = strlen(init_str);
  s.str = strdup(init_str);
  return s;
}

// Append string to the end
inline void str_append(str *s, const char *str) {
  int new_len = s->len + strlen(str);
  s->str = realloc(s->str, new_len + 1);
  strcat(s->str, str);
  s->len = new_len;
}

static inline void str_append_printf(str *s, const char *format, ...) {
  // Initialize a variable argument list
  va_list args;
  va_start(args, format);
  // Get the length of the formatted string
  int formatted_len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  // Allocate enough space for the formatted string
  char *formatted_str = malloc(formatted_len + 1);
  // Write the formatted string into the allocated space
  va_start(args, format);
  vsnprintf(formatted_str, formatted_len + 1, format, args);
  va_end(args);
  // Append the formatted string to the existing string
  str_append(s, formatted_str);
  // Free the temporary formatted string
  free(formatted_str);
}

// Prepend string to the beginning
inline void str_prepend(str *s, const char *str) {
  int new_len = s->len + strlen(str);
  s->str = realloc(s->str, new_len + 1);
  memmove(s->str + strlen(str), s->str, s->len + 1);
  memcpy(s->str, str, strlen(str));
  s->len = new_len;
}

static inline void str_remove(str *s, const char *str_to_remove) {
  int str_to_remove_len = strlen(str_to_remove);
  if (str_to_remove_len == 0)
    return; // Nothing to remove
  // Calculate the new length after removal
  int count = 0;
  const char *temp = s->str;
  while ((temp = strstr(temp, str_to_remove)) != NULL) {
    count++;
    temp += str_to_remove_len;
  }
  if (count == 0)
    return; // No occurrences found
  // Allocate memory for the new string
  int new_len = s->len - count * str_to_remove_len;
  char *new_str = malloc(new_len + 1);
  // Remove occurrences
  char *current_pos = new_str;
  const char *src_pos = s->str;
  while ((temp = strstr(src_pos, str_to_remove)) != NULL) {
    // Copy the part before the occurrence
    int bytes_to_copy = temp - src_pos;
    memcpy(current_pos, src_pos, bytes_to_copy);
    current_pos += bytes_to_copy;
    // Move past the substring to remove
    src_pos = temp + str_to_remove_len;
  }
  // Copy the remaining part of the original string
  strcpy(current_pos, src_pos);
  // Update the str struct
  free(s->str); // Free the old string memory
  s->str = new_str;
  s->len = new_len;
}

inline bool str_contains(str *s, const char *str) { return (bool)strstr(s->str, str); }

inline void str_replace(str *s, const char *str_to_replace, const char *str_replace_with) {
  int str_to_replace_len = strlen(str_to_replace);
  int str_replace_with_len = strlen(str_replace_with);
  // Count the number of occurances of str_to_replace and return if none found
  int count = 0;
  const char *temp = s->str;
  while ((temp = strstr(temp, str_to_replace)) != NULL) {
    count++;
    temp += str_to_replace_len;
  }
  if (count == 0)
    return;
  // Calculate new length
  int new_len = s->len + count * (str_replace_with_len - str_to_replace_len);
  // Create new string
  char *new_str = malloc(new_len + 1);
  // Replace occurrences
  char *current_pos = new_str;
  temp = s->str;
  while ((temp = strstr(temp, str_to_replace)) != NULL) {
    // Copy the part before the occurrence
    int bytes_to_copy = temp - s->str;
    memcpy(current_pos, s->str, bytes_to_copy);
    current_pos += bytes_to_copy;
    // Copy the replacement string
    memcpy(current_pos, str_replace_with, str_replace_with_len);
    current_pos += str_replace_with_len;
    // Move past the replaced substring
    temp += str_to_replace_len;
    s->str = (char *)temp; // Update the original string pointer
  }
  // Copy the remaining part of the original string
  strcpy(current_pos, s->str);
  // Update the str struct
  free(s->str); // Free the old string memory
  s->str = new_str;
  s->len = new_len;
}

// Get null-terminated, newly allocated string slice.
// Returns NULL if error is occured.
inline char *str_slice(str *s, int start_idx, int end_idx) {
  if (end_idx > s->len - 1 || start_idx > end_idx || start_idx < 0 || end_idx < 0)
    return NULL;
  int size = end_idx - start_idx + 1;
  char *slice = malloc(size);
  slice[size] = '\0';
  for (int i = start_idx; i != end_idx; i++)
    slice[i] = s->str[i];
  return slice;
}

// Check if strings are equal
inline bool str_eq(str *s1, str *s2) {
  if (s1->len != s2->len)
    return false;
  return !strcmp(s1->str, s2->str) ? true : false;
}

// Print string
inline void str_print(str *s) { printf("%s\n", s->str); }

// Free the string memory
inline void str_free(str *s) {
  free(s->str);
  s->str = NULL;
  s->len = 0;
}
