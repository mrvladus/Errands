/*

 ███████████    ███████       ███████    █████       ███████████     ███████    █████ █████
░█░░░███░░░█  ███░░░░░███   ███░░░░░███ ░░███       ░░███░░░░░███  ███░░░░░███ ░░███ ░░███
░   ░███  ░  ███     ░░███ ███     ░░███ ░███        ░███    ░███ ███     ░░███ ░░███ ███
    ░███    ░███      ░███░███      ░███ ░███        ░██████████ ░███      ░███  ░░█████
    ░███    ░███      ░███░███      ░███ ░███        ░███░░░░░███░███      ░███   ███░███
    ░███    ░░███     ███ ░░███     ███  ░███      █ ░███    ░███░░███     ███   ███ ░░███
    █████    ░░░███████░   ░░░███████░   ███████████ ███████████  ░░░███████░   █████ █████
   ░░░░░       ░░░░░░░       ░░░░░░░    ░░░░░░░░░░░ ░░░░░░░░░░░     ░░░░░░░    ░░░░░ ░░░░░

*/

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

// -------------------- AUTOMATIC CLEANUP -------------------- //

#if defined(__GNUC__) || defined(__clang__)
// Automatically free memory with custom free function.
// Usage:
//      autofree_f(my_free) char *str = my_malloc(69);
#define autofree_f(free_func) __attribute__((cleanup(free_func)))
// Automatically free memory allocated by malloc, calloc, realloc, strdup, etc.
// Usage:
//      autofree char *str = strdup("Hello, World!");
#define autofree autofree_f(__autofree_free_func)
static inline void __autofree_free_func(void *p) {
  void **pp = (void **)p;
  if (*pp) free(*pp);
}
// If you have custom free func for your type you need to define this once like this:
//      AUTOPTR_DEFINE(my_type, my_type_free_func)
#define AUTOPTR_DEFINE(type, free_func)                                                                                \
  static inline void autoptr_##type##_free_func(type **_ptr) {                                                         \
    if (*_ptr) free_func(*_ptr);                                                                                       \
  }
// After using `AUTOPTR_DEFINE` you can use it like this:
//      autoptr(my_type) foo = my_type_new();
#define autoptr(type) autofree_f(autoptr_##type##_free_func) type *

// Placeholders if compiler doesn't support automatic cleanup
#else
#define autofree_f(free_func)
#define autofree autofree_f(free)
#define AUTOPTR_DECL(type, free_func)
#define autoptr(type) type *
#endif

// -------------------- MATH -------------------- //

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// -------------------- LOG AND DEBUG -------------------- //

// Define this somewhere in your code if you want to add prefix to your log messages.
extern const char *toolbox_log_prefix;
// Print formatted message.
#define LOG(format, ...) fprintf(stderr, "%s" format "\n", toolbox_log_prefix ? toolbox_log_prefix : "", ##__VA_ARGS__)
// Print formatted message without newline.
#define LOG_NO_LN(format, ...)           fprintf(stderr, "%s" format, toolbox_log_prefix ? toolbox_log_prefix : "", ##__VA_ARGS__)
#define LOG_NO_PREFIX(format, ...)       fprintf(stderr, format "\n", ##__VA_ARGS__)
#define LOG_NO_PREFIX_NO_LN(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
// Print formatted message with filename, line number and function name.
#define LOG_DEBUG(format, ...) LOG("%s:%d:%s: " format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
// The best debug method.
#define HERE LOG_DEBUG("HERE")
// Print TODO formatted message.
#define TODO(format, ...) LOG_DEBUG("TODO: " format, ##__VA_ARGS__)
// Prints message and aborts.
#define ABORT(msg)                                                                                                     \
  do {                                                                                                                 \
    LOG_DEBUG(msg);                                                                                                    \
    abort();                                                                                                           \
  } while (0)
// Prints 'UNIMPLEMENTED' message and aborts.
#define UNIMPLEMENTED ABORT("UNIMPLEMENTED")
// Prints 'PANIC' message and aborts.
#define PANIC ABORT("PANIC")
// Prints 'UNREACHABLE' message and aborts.
#define UNREACHABLE ABORT("UNREACHABLE")
// Breakpoint for debugger
#define BREAKPOINT asm("int3")

// -------------------- PROFILING -------------------- //

#define PROFILE_START clock_t __start_time = clock()
#define PROFILE_END   LOG_DEBUG("%f sec.", (double)(clock() - __start_time) / CLOCKS_PER_SEC)

// -------------------- LOOPS -------------------- //

// --- FOR LOOPS --- //

#define for_ever                                      for (;;)
#define for_range(idx, idx_start, idx_end)            for (int64_t idx = idx_start; idx < idx_end; ++idx)
#define for_range_reverse(idx, idx_start, idx_end)    for (int64_t idx = idx_end - 1; idx >= idx_start; --idx)
#define for_range_step(idx, idx_start, idx_end, step) for (int64_t idx = idx_start; idx < idx_end; idx += step)
#define for_range_step_reverse(idx, idx_start, idx_end, step)                                                          \
  for (int64_t idx = idx_end - 1; idx >= idx_start; idx -= step)

// --- INSIDE LOOPS --- //

#define CONTINUE_IF(statement)                                                                                         \
  if (statement) continue;

// -------------------- STRING -------------------- //

// Check if strings are equal.
#define STR_EQUAL(s1, s2) (strcmp((const char *)(s1), (const char *)(s2)) == 0)
// Check if string contains substring.
#define STR_CONTAINS(s1, s2) (strstr((const char *)(s1), (const char *)(s2)) != NULL)
// Check if string contains substring (case-insensitive).
#define STR_CONTAINS_CASE(s1, s2) (strcasestr((const char *)(s1), (const char *)(s2)) != NULL)

// -------------------- DYNAMIC ARRAY -------------------- //

// Dynamic array struct
typedef struct {
  void **items;
  size_t len;
  size_t capacity;
  void (*item_free_func)(void *);
} tb_array;

// Create new array struct with `initial_capacity`
static inline tb_array tb_array_new(size_t initial_capacity) {
  tb_array array = {
      .items = calloc(1, sizeof(void *) * initial_capacity),
      .len = 0,
      .capacity = initial_capacity,
      .item_free_func = NULL,
  };
  return array;
}

// Free `array` and its items if `item_free_func` is provided
static inline void tb_array_free(tb_array *array) {
  if (array->item_free_func)
    for (size_t i = 0; i < array->len; i++) array->item_free_func(array->items[i]);
  if (array->items) free(array->items);
}

// Create new array struct with `initial_capacity` and `item_free_func`
static inline tb_array tb_array_new_with_free_func(size_t initial_capacity, void (*item_free_func)(void *)) {
  tb_array array = tb_array_new(initial_capacity);
  array.item_free_func = item_free_func;
  return array;
}

// Add `item` to `array`
static inline void tb_array_add(tb_array *array, void *item) {
  if (array->capacity == array->len) {
    array->capacity *= 2;
    array->items = realloc(array->items, array->capacity * sizeof(void *));
  }
  array->items[array->len++] = item;
}

// Insert `item` at `idx` in `array`
static inline void tb_array_insert(tb_array *array, size_t idx, void *item) {
  if (array->capacity == array->len) {
    array->capacity *= 2;
    array->items = realloc(array->items, array->capacity * sizeof(void *));
  }
  for (size_t i = array->len; i > idx; i--) array->items[i] = array->items[i - 1];
  array->items[idx] = item;
  array->len++;
}

// Remove and free last item from `array`
static inline void tb_array_pop(tb_array *array) {
  if (array->item_free_func) array->item_free_func(array->items[array->len - 1]);
  array->len--;
}

// Remove last item from `array` and return it without freeing it.
// User is responsible for freeing the returned item.
static inline void *tb_array_steal(tb_array *array) {
  if (array->len == 0) return NULL;
  void *item = array->items[array->len - 1];
  array->len--;
  return item;
}

// Remove item at `idx` from `array` and return it without freeing it.
// Move items after idx.
// User is responsible for freeing the returned item.
static inline void *tb_array_steal_idx(tb_array *array, size_t idx) {
  if (array->len == 0) return NULL;
  void *item = array->items[idx];
  for (size_t i = idx; i < array->len - 1; i++) array->items[i] = array->items[i + 1];
  array->len--;
  return item;
}

// Get last item of the `array`
static inline void *tb_array_last(tb_array *array) {
  if (array->len == 0) return NULL;
  return array->items[array->len - 1];
}

// -------------------- TEMPORARY STRING -------------------- //

/*

This is temporary buffer for printing formatted strings.
If you need to get formatted string but not wanting to use malloc, use this.
Default size is 8192 bytes. You can change it by defining TB_TMP_STR_BUFFER_SIZE.
When buffer is full - starts overriding buffer from the beginning.

To use this define TOOLBOX_TMP_STR in one file AFTER #include "toolbox.h"

*/

#ifndef TB_TMP_STR_BUFFER_SIZE
#define TB_TMP_STR_BUFFER_SIZE 8192
#endif // TB_TMP_STR_BUFFER_SIZE

#define TOOLBOX_TMP_STR                                                                                                \
  char toolbox_tmp_str[TB_TMP_STR_BUFFER_SIZE];                                                                        \
  size_t toolbox_tmp_str_offset = 0;

extern char toolbox_tmp_str[TB_TMP_STR_BUFFER_SIZE];
extern size_t toolbox_tmp_str_offset;

static inline const char *tmp_str_printf(const char *format, ...) {
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
  if (len >= (size_t)TB_TMP_STR_BUFFER_SIZE - toolbox_tmp_str_offset - 1) toolbox_tmp_str_offset = 0;
  // Print the string
  vsnprintf(toolbox_tmp_str + toolbox_tmp_str_offset, (size_t)TB_TMP_STR_BUFFER_SIZE - toolbox_tmp_str_offset, format,
            args);
  const char *result = toolbox_tmp_str + toolbox_tmp_str_offset;
  toolbox_tmp_str_offset += len + 1;
  va_end(args);
  return result;
}

// -------------------- FILE FUNCTIONS -------------------- //

static inline bool file_exists(const char *path) {
  FILE *file = fopen(path, "r");
  if (file) {
    fclose(file);
    return true;
  }
  return false;
}

static inline bool dir_exists(const char *path) {
  struct stat statbuf;
  return (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
}

static inline char *read_file_to_string(const char *path) {
  FILE *file = fopen(path, "r");
  if (!file) return NULL;
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *buf = (char *)malloc(file_size + 1);
  fread(buf, 1, file_size, file);
  buf[file_size] = '\0';
  fclose(file);
  return buf;
}

static inline bool write_string_to_file(const char *path, const char *str) {
  FILE *file = fopen(path, "w");
  if (file) {
    fprintf(file, "%s", str);
    fclose(file);
    return true;
  }
  return false;
}

// -------------------- PATH FUNCTIONS -------------------- //

// Get path base name. e. g. "/home/user/file.txt" -> "file.txt".
// Returns pointer to the beginning of the base name in the path or NULL on error.
static inline const char *tb_path_base_name(const char *path) {
  const char *last_sep = strrchr(path, '/');
  if (!last_sep) last_sep = strrchr(path, '\\');
  return last_sep ? last_sep + 1 : path;
}
// Get path extension. e. g. "/home/user/file.txt" -> "txt".
// Returns pointer to the beginning of the extension in the path or NULL on error.
static inline const char *tb_path_ext(const char *path) {
  const char *last_dot = strrchr(path, '.');
  return last_dot ? last_dot + 1 : NULL;
}

// -------------------- TIME -------------------- //

// Get current time
#define TIME_NOW time(NULL)
// Start timer
#define TIMER_START clock_t __timer_start = clock();
// Get elapsed time in milliseconds
#define TIMER_ELAPSED_MS ((double)(clock() - __timer_start) / CLOCKS_PER_SEC)

// -------------------- COLORS -------------------- //

static inline char *generate_hex_as_str() {
  static char hex[8] = {0};
  sprintf(hex, "#%06x", rand() % 0xFFFFFF);
  return hex;
}

#endif // TOOLBOX_H
