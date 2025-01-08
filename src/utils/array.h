#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// Dynamic array of pointers
typedef struct {
  void **data;
  size_t len;
  size_t capacity;
} PtrArray;

// Create dynamic array
static inline PtrArray *array_new(size_t initial_capacity) {
  PtrArray *array = malloc(sizeof(PtrArray));
  if (!array) {
    printf("Failed to allocate memory for array structure");
    return NULL;
  }
  array->data = malloc(initial_capacity * sizeof(void *));
  if (!array->data) {
    printf("Failed to allocate memory for the array data");
    free(array);
    return NULL;
  }
  array->len = 0;
  array->capacity = initial_capacity;
  return array;
}

// Increase array capacity by 2
static inline void array_resize(PtrArray *array) {
  size_t new_capacity = array->capacity * 2;
  void **new_data = realloc(array->data, new_capacity * sizeof(void *));
  if (!new_data) {
    printf("Failed to reallocate memory for array");
    return;
  }
  array->data = new_data;
  array->capacity = new_capacity;
}

// Add an element to the array
static inline void array_append(PtrArray *a, void *data) {
  if (a->len == a->capacity)
    array_resize(a);
  a->data[a->len++] = data;
}

// Add an element to the beginning of the array
static inline void array_prepend(PtrArray *a, void *data) {
  if (a->len == a->capacity)
    array_resize(a);
  for (size_t i = a->len; i > 0; i--)
    a->data[i] = a->data[i - 1];
  a->data[0] = data;
  a->len++;
}

// Get an element from the array at a given index
static inline void *array_at(PtrArray *a, size_t index) {
  if (index >= a->len) {
    printf("Index out of bounds\n");
    return NULL;
  }
  return a->data[index];
}

// Find the index of a given data pointer, or -1 if not found
static inline int array_index(PtrArray *a, void *data) {
  for (size_t i = 0; i < a->len; i++)
    if (a->data[i] == data)
      return (int)i;
  return -1;
}

// Swap elements at two indices in the array
static inline void array_swap_index(PtrArray *a, size_t index1, size_t index2) {
  if (index1 >= a->len || index2 >= a->len) {
    printf("Index out of bounds\n");
    return;
  }
  void *temp = a->data[index1];
  a->data[index1] = a->data[index2];
  a->data[index2] = temp;
}

// Swap the positions of two elements specified by their data pointers
static inline void array_swap_data(PtrArray *array, void *data1, void *data2) {
  size_t index1 = -1, index2 = -1;
  // Find the indices of data1 and data2 in the array
  for (size_t i = 0; i < array->len; i++) {
    if (array->data[i] == data1)
      index1 = i;
    if (array->data[i] == data2)
      index2 = i;
  }
  // If either data1 or data2 are not found, return without doing anything
  if (index1 == (size_t)-1 || index2 == (size_t)-1) {
    printf("One or both elements not found\n");
    return;
  }
  // Swap the elements at the found indices
  void *temp = array->data[index1];
  array->data[index1] = array->data[index2];
  array->data[index2] = temp;
}

// Move an element at 'index_to_move' before the element at 'index_to_move_before'
static inline void array_move_index_before(PtrArray *array, size_t index_to_move,
                                           size_t index_to_move_before) {
  if (index_to_move >= array->len || index_to_move_before >= array->len) {
    printf("Index out of bounds\n");
    return;
  }
  if (index_to_move == index_to_move_before)
    return; // No need to move if the indices are the same
  void *temp = array->data[index_to_move];
  // Shift elements between index_to_move_before and index_to_move one step to the right
  if (index_to_move < index_to_move_before)
    for (size_t i = index_to_move; i < index_to_move_before; i++)
      array->data[i] = array->data[i + 1];
  else
    // Shift elements between index_to_move_before and index_to_move one step to the left
    for (size_t i = index_to_move; i > index_to_move_before; i--)
      array->data[i] = array->data[i - 1];
  // Place the element at its new position
  array->data[index_to_move_before] = temp;
}

// Move an element at 'index_to_move' after the element at 'index_to_move_after'
static inline void array_move_index_after(PtrArray *array, size_t index_to_move,
                                          size_t index_to_move_after) {
  if (index_to_move >= array->len || index_to_move_after >= array->len) {
    printf("Index out of bounds\n");
    return;
  }
  if (index_to_move == index_to_move_after)
    return; // No need to move if the indices are the same
  void *temp = array->data[index_to_move];
  // Shift elements between index_to_move_after and index_to_move one step to the left
  if (index_to_move > index_to_move_after)
    for (size_t i = index_to_move; i > index_to_move_after + 1; i--)
      array->data[i] = array->data[i - 1];
  else
    // Shift elements between index_to_move_after and index_to_move one step to the left
    for (size_t i = index_to_move; i < index_to_move_after; i++)
      array->data[i] = array->data[i + 1];
  // Place the element at its new position
  array->data[index_to_move_after + 1] = temp;
}

// Remove and return the element at the specified index
static inline void *array_pop_at(PtrArray *array, size_t index) {
  if (index >= array->len) {
    printf("Index out of bounds\n");
    return NULL;
  }
  void *popped_data = array->data[index];
  // Shift elements to the left to fill the gap
  for (size_t i = index; i < array->len - 1; i++)
    array->data[i] = array->data[i + 1];
  array->len--;
  return popped_data;
}

// Remove the first occurrence of the specified element.
// Return true for successful operation.
// Return false if the element was not found.
static inline bool array_remove(PtrArray *array, void *data) {
  for (size_t i = 0; i < array->len; i++)
    if (array->data[i] == data) {
      void *element = array_pop_at(array, i);
      free(element);
      element = NULL;
      return true;
    }
  return false;
}

// Free the array. If free_data is true - call free() on each element.
static inline void array_free(PtrArray *array, bool free_data) {
  if (free_data)
    for (size_t i = 0; i < array->len; i++)
      free(array->data[i]);
  free(array->data);
  free(array);
}

// Generic dynamic array
#define DECLARE_DYNAMIC_ARRAY(type, prefix)                                                        \
  typedef struct {                                                                                 \
    size_t len;                                                                                    \
    size_t capacity;                                                                               \
    type *data;                                                                                    \
  } type##_array;                                                                                  \
                                                                                                   \
  static inline type##_array prefix##_array_new() {                                                \
    type##_array array;                                                                            \
    array.len = 0;                                                                                 \
    array.capacity = 2;                                                                            \
    array.data = malloc(sizeof(type##_array) * array.capacity);                                    \
    return array;                                                                                  \
  }                                                                                                \
                                                                                                   \
  static inline void prefix##_array_append(type##_array *array, type data) {                       \
    if (array->len == array->capacity) {                                                           \
      array->capacity *= 2;                                                                        \
      type *new_data = realloc(array->data, array->capacity * sizeof(type##_array));               \
      if (!new_data) {                                                                             \
        printf("Failed to reallocate memory for array");                                           \
        return;                                                                                    \
      }                                                                                            \
      array->data = new_data;                                                                      \
    }                                                                                              \
    array->data[array->len++] = data;                                                              \
  }
