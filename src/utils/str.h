#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- DYNAMIC STRING ---------- //

// Struct to hold string data
typedef struct {
  char *str;
  int len;
} str;

// Creates new string
static inline str str_new(const char *init_str) {
  str s;
  s.len = strlen(init_str);
  s.str = strdup(init_str);
  return s;
}

// Creates new formatted string
static inline str str_new_printf(const char *format, ...) {
  str s;
  // Initialize a variable argument list
  va_list args;
  va_start(args, format);
  // Calculate the required length for the formatted string
  int formatted_len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  // Allocate memory for the formatted string
  s.str = malloc(formatted_len + 1);
  if (!s.str) {
    s.len = 0;
    return s; // Allocation failed, return an empty struct
  }
  // Write the formatted string into the allocated space
  va_start(args, format);
  vsnprintf(s.str, formatted_len + 1, format, args);
  va_end(args);
  // Set the length of the string
  s.len = formatted_len;
  return s;
}

// Append string to the end
static inline void str_append(str *s, const char *str) {
  int new_len = s->len + strlen(str);
  s->str = realloc(s->str, new_len + 1);
  strcat(s->str, str);
  s->len = new_len;
}

// Append formatted string to the end
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
static inline void str_prepend(str *s, const char *str) {
  int new_len = s->len + strlen(str);
  s->str = realloc(s->str, new_len + 1);
  memmove(s->str + strlen(str), s->str, s->len + 1);
  memcpy(s->str, str, strlen(str));
  s->len = new_len;
}

// Remove all occurrences of str_to_remove in string
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

// Check if string contains sub-string
static inline bool str_contains(str *s, const char *str) { return (bool)strstr(s->str, str); }

// Replace all occurrences of str_to_replace with str_replace_with
static inline void str_replace(str *s, const char *str_to_replace, const char *str_replace_with) {
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
static inline char *str_slice(str *s, int start_idx, int end_idx) {
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
static inline bool str_eq(str *s1, str *s2) {
  if (s1->len != s2->len)
    return false;
  return !strcmp(s1->str, s2->str) ? true : false;
}

// Check if str is equal to C string
static inline bool str_eq_c(str *s1, const char *s2) { return !strcmp(s1->str, s2) ? true : false; }

// Print string
static inline void str_print(str *s) { printf("%s\n", s->str); }

// Free the string memory
static inline void str_free(str *s) {
  free(s->str);
  s->str = NULL;
  s->len = 0;
}
