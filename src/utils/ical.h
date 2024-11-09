#pragma once

#include <glib.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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
