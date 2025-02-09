#pragma once

#include <gtk/gtk.h>

#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>

// Log formatted message
#define LOG(format, ...) fprintf(stderr, "\033[0;32m[Errands] \033[0m" format "\n", ##__VA_ARGS__)
#define ERROR(format, ...) fprintf(stderr, "\033[1;31m[Errands Error] " format "\033[0m\n", ##__VA_ARGS__)

// For range from start to end - 1
#define for_range(var, start, end) for (int var = start; var < end; var++)

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

static inline void gtk_box_remove_all(GtkWidget *box) {
  g_autoptr(GPtrArray) children = get_children(box);
  for (int i = 0; i < children->len; i++)
    gtk_box_remove(GTK_BOX(box), GTK_WIDGET(children->pdata[i]));
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

static inline void generate_hex(char *color) {
  srand(time(NULL));
  int red = rand() % 256;
  int green = rand() % 256;
  int blue = rand() % 256;
  sprintf(color, "#%02X%02X%02X", red, green, blue);
  LOG("Generate color '%s'", color);
}

static inline void gdk_rgba_to_hex_string(const GdkRGBA *rgba, char *hex_string) {
  // Convert the RGBA components to integers in the range [0, 255]
  int r = (int)(rgba->red * 255);
  int g = (int)(rgba->green * 255);
  int b = (int)(rgba->blue * 255);
  // Format the string as #RRGGBB
  sprintf(hex_string, "#%02X%02X%02X", r, g, b);
}

// Add shortcut to the widget
static inline void errands_add_shortcut(GtkWidget *widget, const char *trigger, const char *action) {
  GtkEventController *ctrl = gtk_shortcut_controller_new();
  gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(ctrl), GTK_SHORTCUT_SCOPE_GLOBAL);
  GtkShortcut *sc =
      gtk_shortcut_new(gtk_shortcut_trigger_parse_string(trigger), gtk_shortcut_action_parse_string(action));
  gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ctrl), sc);
  gtk_widget_add_controller(widget, ctrl);
}

static inline void generate_uuid(char *uuid) {
  gchar *uid = g_uuid_string_random();
  strcpy(uuid, uid);
  g_free(uid);
}

static inline void g_ptr_array_move_before(GPtrArray *array, gpointer element, gpointer target) {
  gint index = -1;
  gint target_index = -1;

  // Find the indices of the element to move and the target element
  for (gint i = 0; i < array->len; i++) {
    if (g_ptr_array_index(array, i) == element) {
      index = i;
    }
    if (g_ptr_array_index(array, i) == target) {
      target_index = i;
    }
  }

  // Check if both indices were found
  if (index == -1 || target_index == -1 || index == target_index) {
    g_print("Element or target not found, or they are the same.\n");
    return;
  }

  // Remove the element from its current position
  gpointer temp = g_ptr_array_index(array, index);
  g_ptr_array_remove_index(array, index);

  // Adjust the target index if the element was before it
  if (index < target_index) {
    target_index--; // Adjust because we removed the element
  }

  // Insert the element before the target
  g_ptr_array_insert(array, target_index, temp);
}

static inline void g_ptr_array_move_after(GPtrArray *array, gpointer element, gpointer target) {
  gint index = -1;
  gint target_index = -1;

  // Find the indices of the element to move and the target element
  for (gint i = 0; i < array->len; i++) {
    if (g_ptr_array_index(array, i) == element) {
      index = i;
    }
    if (g_ptr_array_index(array, i) == target) {
      target_index = i;
    }
  }

  // Check if both indices were found
  if (index == -1 || target_index == -1 || index == target_index) {
    g_print("Element or target not found, or they are the same.\n");
    return;
  }

  // Remove the element from its current position
  gpointer temp = g_ptr_array_index(array, index);
  g_ptr_array_remove_index(array, index);

  // Adjust the target index if the element was before it
  if (index < target_index) {
    target_index--; // Adjust because we removed the element
  }

  // Insert the element after the target
  g_ptr_array_insert(array, target_index + 1, temp);
}

static inline void get_date_time(char date_time[17]) {
  g_autoptr(GDateTime) dt = g_date_time_new_now_local();
  g_autofree gchar *tmp = g_date_time_format(dt, "%Y%m%dT%H%M%SZ");
  strcpy(date_time, tmp);
}

static inline char *get_today_date() {
  GDateTime *dt = g_date_time_new_now_local();
  static char date[9];
  char *tmp = g_date_time_format(dt, "%Y%m%d");
  strcpy(date, tmp);
  g_free(tmp);
  g_date_time_unref(dt);
  return date;
}

// Helper function to parse the due date into a GDateTime
static inline GDateTime *parse_date(const char *date) {
  if (strlen(date) == 8) { // Format: "YYYYMMDD"
    char year[5], month[3], day[3];
    strncpy(year, date, 4);
    strncpy(month, date + 4, 2);
    strncpy(day, date + 6, 2);
    year[4] = '\0';
    month[2] = '\0';
    day[2] = '\0';
    char dt[17];
    sprintf(dt, "%s%s%sT000000Z", year, month, day);
    return g_date_time_new_from_iso8601(dt, NULL);
  } else if (strlen(date) == 16) { // Format: "YYYYMMDDTHHMMSSZ"
    return g_date_time_new_from_iso8601(date, NULL);
  }
  return NULL;
}

static inline char *read_file_to_string(const char *path) {
  FILE *file = fopen(path, "r"); // Open the file in read mode
  if (!file) {
    LOG("Could not open file"); // Print error if file cannot be opened
    return NULL;
  }
  // Move the file pointer to the end of the file to get the size
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file); // Get the current position (file size)
  fseek(file, 0, SEEK_SET);     // Move back to the beginning of the file
  // Allocate memory for the string (+1 for the null terminator)
  char *buf = (char *)malloc(file_size + 1);
  fread(buf, 1, file_size, file);
  buf[file_size] = '\0'; // Null-terminate the string
  fclose(file);
  return buf;
}

static inline void write_string_to_file(const char *path, const char *str) {
  FILE *file = fopen(path, "w");
  if (file) {
    fprintf(file, "%s", str);
    fclose(file);
  }
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

// ---------- ICAL ---------- //

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

static inline char *get_ical_value(const char *ical, const char *key) {
  const char *val_start = strstr(ical, key);
  if (!val_start)
    return NULL;
  val_start += strlen(key) + 1;
  const char *val_end = strchr(val_start, '\n');
  if (!val_end)
    return NULL;
  const int val_len = val_end - val_start;
  char *value = malloc(val_len);
  value[val_len] = '\0';
  strncpy(value, val_start, val_len);
  return value;
}

static inline GPtrArray *get_vtodos(const char *ical) {
  GPtrArray *vtodo_array = g_ptr_array_new();
  const char *vtodo_start = "BEGIN:VTODO";
  const char *vtodo_end = "END:VTODO";
  const char *current_pos = ical;
  while ((current_pos = strstr(current_pos, vtodo_start)) != NULL) {
    const char *end_pos = strstr(current_pos, vtodo_end);
    if (end_pos == NULL) {
      break; // No matching END found
    }
    // Calculate the length of the VTODO entry
    size_t vtodo_length = end_pos + strlen(vtodo_end) - current_pos;
    // Allocate memory for the VTODO string
    char *vtodo_entry = (char *)malloc(vtodo_length + 1);
    if (vtodo_entry == NULL) {
      g_ptr_array_free(vtodo_array, TRUE);
      return NULL; // Memory allocation failed
    }
    // Copy the VTODO entry into the allocated memory
    strncpy(vtodo_entry, current_pos, vtodo_length);
    vtodo_entry[vtodo_length] = '\0'; // Null-terminate the string
    // Add the VTODO entry to the GPtrArray
    g_ptr_array_add(vtodo_array, vtodo_entry);
    // Move the current position past the end of the current VTODO entry
    current_pos = end_pos + strlen(vtodo_end);
  }

  return vtodo_array;
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

// Generic dynamic array
#define DECLARE_DYNAMIC_ARRAY(type, item_type, func_prefix)                                                            \
  typedef struct type {                                                                                                \
    size_t len;                                                                                                        \
    size_t capacity;                                                                                                   \
    item_type *data;                                                                                                   \
  } type;                                                                                                              \
                                                                                                                       \
  static inline type func_prefix##_array_new() {                                                                       \
    type array;                                                                                                        \
    array.len = 0;                                                                                                     \
    array.capacity = 2;                                                                                                \
    array.data = malloc(sizeof(type) * array.capacity);                                                                \
    return array;                                                                                                      \
  }                                                                                                                    \
                                                                                                                       \
  static inline void func_prefix##_array_append(type *array, type data) {                                              \
    if (array->len == array->capacity) {                                                                               \
      array->capacity *= 2;                                                                                            \
      type *new_data = realloc(array->data, array->capacity * sizeof(type));                                           \
      if (!new_data) {                                                                                                 \
        printf("Failed to reallocate memory for array");                                                               \
        return;                                                                                                        \
      }                                                                                                                \
      array->data = new_data;                                                                                          \
    }                                                                                                                  \
    array->data[array->len++] = data;                                                                                  \
  }
