#ifndef TOOLBOX_H
#define TOOLBOX_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// -------------------- LOG AND DEBUG -------------------- //

extern const char *tb_log_prefix;

// Print formatted message.
void tb_log(const char *format, ...);

// Print formatted message with filename, line number and function name.
#define TB_LOG_DEBUG(format, ...)                                                                                      \
  tb_log("%s:%d:%s: " format, tb_path_base_name(__FILE__), __LINE__, __func__, ##__VA_ARGS__)

// The best debug method.
#define TB_HERE TB_LOG_DEBUG("HERE")

// Print TODO formatted message.
#define TB_TODO(format, ...) TB_LOG_DEBUG("TODO: " format, ##__VA_ARGS__)

// Prints message and aborts.
#define TB_ABORT(msg)                                                                                                  \
  do {                                                                                                                 \
    TB_LOG_DEBUG(msg);                                                                                                 \
    abort();                                                                                                           \
  } while (0)

// Prints 'UNIMPLEMENTED' message and aborts.
#define TB_UNIMPLEMENTED TB_ABORT("UNIMPLEMENTED")

// Prints 'PANIC' message and aborts.
#define TB_PANIC TB_ABORT("PANIC")

// Prints 'UNREACHABLE' message and aborts.
#define TB_UNREACHABLE TB_ABORT("UNREACHABLE")

// Profile block of code
//
//   Usage:
//
//   TB_PROFILE {
//       code();
//        to();
//       profile();
//   }
#define TB_PROFILE                                                                                                     \
  for (clock_t __start_time = clock(), i = 0; i < 1;                                                                   \
       ++i, TB_LOG_DEBUG("%f sec.", (double)(clock() - __start_time) / CLOCKS_PER_SEC))

// -------------------- LOOPS MACROS -------------------- //

#define for_range(idx, idx_start, idx_end) for (int64_t idx = idx_start; idx < idx_end; ++idx)

// -------------------- DYNAMIC ARRAY -------------------- //

// Dynamic array struct
typedef struct {
  void **items;
  size_t len;
  size_t capacity;
  void (*item_free_func)(void *);
} tb_array;

// Create new array struct with `initial_capacity`
tb_array tb_array_new(size_t initial_capacity);
// Create new array struct with `initial_capacity` and `item_free_func`
tb_array tb_array_new_full(size_t initial_capacity, void (*item_free_func)(void *));
// Free `array` and its items if `item_free_func` is provided
void tb_array_free(tb_array *array);
// Add `item` to `array`
void tb_array_add(tb_array *array, void *item);
// Remove and free last item from `array`
void tb_array_pop(tb_array *array);
// Remove last item from `array` and return it without freeing it.
// User is responsible for freeing the returned item.
void *tb_array_steal(tb_array *array);
// Remove item at `idx` from `array` and return it without freeing it.
// Move items after idx.
// User is responsible for freeing the returned item.
void *tb_array_steal_idx(tb_array *array, size_t idx);
void *tb_array_last(tb_array *array);

// -------------------- TEMPORARY STRING -------------------- //

/*

This is temporary buffer for printing formatted strings.
If you need to get formatted string but not wanting to use malloc, use this.
Default size is 8192 bytes. You can change it by defining TB_TMP_STR_BUFFER_SIZE.
When buffer is full - start override buffer from the beginning.

*/

#ifndef TB_TMP_STR_BUFFER_SIZE
#define TB_TMP_STR_BUFFER_SIZE 8192
#endif // TB_TMP_STR_BUFFER_SIZE

const char *tb_tmp_str_printf(const char *format, ...);

// -------------------- FILE FUNCTIONS -------------------- //

bool tb_file_exists(const char *path);
bool tb_dir_exists(const char *path);
char *tb_read_file_to_string(const char *path);
void tb_write_string_to_file(const char *path, const char *str);

// -------------------- PATH FUNCTIONS -------------------- //

// Get path base name. e. g. "/home/user/file.txt" -> "file.txt".
// Returns pointer to the beginning of the base name in the path or NULL on error.
const char *tb_path_base_name(const char *path);
// Get path extension. e. g. "/home/user/file.txt" -> "txt".
// Returns pointer to the beginning of the extension in the path or NULL on error.
const char *tb_path_ext(const char *path);

// -------------------- STRING FUNCTIONS -------------------- //

// Check if string contains substring.
bool tb_str_contains(const char *str, const char *sub_str);
// Check if string contains substring (case-insensitive).
bool tb_str_contains_case(const char *str, const char *sub_str);

// -------------------- TIME FUNCTIONS -------------------- //

time_t tb_time_now();

#endif // TOOLBOX_H

#ifdef TOOLBOX_IMPLEMENTATION

// -------------------- DYNAMIC ARRAY -------------------- //

tb_array tb_array_new(size_t initial_capacity) {
  tb_array array = {
      .items = calloc(1, sizeof(void *) * initial_capacity),
      .len = 0,
      .capacity = initial_capacity,
      .item_free_func = NULL,
  };
  return array;
}

tb_array tb_array_new_full(size_t initial_capacity, void (*item_free_func)(void *)) {
  tb_array array = tb_array_new(initial_capacity);
  array.item_free_func = item_free_func;
  return array;
}

void tb_array_free(tb_array *array) {
  if (array->item_free_func)
    for (size_t i = 0; i < array->len; i++) array->item_free_func(array->items[i]);
  if (array->items) free(array->items);
}

void tb_array_add(tb_array *array, void *item) {
  if (array->capacity == array->len) {
    array->capacity *= 2;
    array->items = realloc(array->items, array->capacity * sizeof(void *));
  }
  array->items[array->len++] = item;
}

void tb_array_pop(tb_array *array) {
  if (array->item_free_func) array->item_free_func(array->items[array->len - 1]);
  array->len--;
}

void *tb_array_steal(tb_array *array) {
  if (array->len == 0) return NULL;
  void *item = array->items[array->len - 1];
  array->len--;
  return item;
}

void *tb_array_steal_idx(tb_array *array, size_t idx) {
  if (array->len == 0) return NULL;
  void *item = array->items[idx];
  for (size_t i = idx; i < array->len - 1; i++) array->items[i] = array->items[i + 1];
  array->len--;
  return item;
}

void *tb_array_last(tb_array *array) {
  if (array->len == 0) return NULL;
  return array->items[array->len - 1];
}

// -------------------- FILE FUNCTIONS -------------------- //

bool tb_file_exists(const char *path) {
  FILE *file = fopen(path, "r");
  if (file) {
    fclose(file);
    return true;
  }
  return false;
}

bool tb_dir_exists(const char *path) {
  struct stat statbuf;
  return (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
}

char *tb_read_file_to_string(const char *path) {
  FILE *file = fopen(path, "r"); // Open the file in read mode
  if (!file) {
    tb_log("Could not open file '%s'", path); // Print error if file cannot be opened
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *buf = (char *)malloc(file_size + 1);
  fread(buf, 1, file_size, file);
  buf[file_size] = '\0';
  fclose(file);
  return buf;
}

void tb_write_string_to_file(const char *path, const char *str) {
  FILE *file = fopen(path, "w");
  if (file) {
    fprintf(file, "%s", str);
    fclose(file);
  }
}

// -------------------- LOG AND DEBUG -------------------- //

const char *tb_log_prefix = "";

void tb_log(const char *format, ...) {
  fprintf(stderr, "%s", tb_log_prefix);
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, "\n");
}

// -------------------- TEMPORARY STRING -------------------- //

const char *tb_tmp_str_printf(const char *format, ...) {
  static char tb_tmp_str_buffer[TB_TMP_STR_BUFFER_SIZE];
  static size_t tb_tmp_str_offset = 0;
  if (!format) return NULL;
  va_list args;
  va_start(args, format);
  va_list args_copy;
  va_copy(args_copy, args);
  int len = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);
  if (len < 0) {
    va_end(args);
    return "";
  }
  if (len >= (size_t)TB_TMP_STR_BUFFER_SIZE - tb_tmp_str_offset - 1) tb_tmp_str_offset = 0;
  // Print the string
  vsnprintf(tb_tmp_str_buffer + tb_tmp_str_offset, (size_t)TB_TMP_STR_BUFFER_SIZE - tb_tmp_str_offset, format, args);
  const char *result = tb_tmp_str_buffer + tb_tmp_str_offset;
  tb_tmp_str_offset += len + 1;
  va_end(args);
  return result;
}

// -------------------- PATH FUNCTIONS -------------------- //

const char *tb_path_base_name(const char *path) {
  const char *last_sep = strrchr(path, '/');
  if (!last_sep) last_sep = strrchr(path, '\\');
  return last_sep ? last_sep + 1 : path;
}

const char *tb_path_ext(const char *path) {
  const char *last_dot = strrchr(path, '.');
  return last_dot ? last_dot + 1 : NULL;
}

// -------------------- STRING FUNCTIONS -------------------- //

bool tb_str_contains(const char *str, const char *sub_str) { return strstr(str, sub_str) != NULL; }
bool tb_str_contains_case(const char *str, const char *sub_str) { return strcasestr(str, sub_str) != NULL; }

// -------------------- TIME FUNCTIONS -------------------- //

time_t tb_time_now() { return time(NULL); }

#endif // TOOLBOX_IMPLEMENTATION
