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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>
#include <time.h>

// -------------------- TYPES -------------------- //

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// -------------------- TYPE CONVERSION -------------------- //

#define BOOL_TO_STR(val)     (val) ? "true" : "false"
#define BOOL_TO_STR_NUM(val) (val) ? "1" : "0"
#define STR_TO_BOOL(str)     (!strcmp((const char *)(str), "1") || !strcmp((const char *)(str), "true"))
#define STR_TO_UL(str)       strtoul(str, NULL, 10)
#define I32_TO_VOIDP(val)    ((void *)(i32)(val))
#define U32_TO_VOIDP(val)    ((void *)(u32)(val))
#define VOIDP_TO_I32(ptr)    ((int)(i32)(ptr))
#define VOIDP_TO_U32(ptr)    ((unsigned int)(u32)(ptr))

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

// Placeholders when compiler doesn't support automatic cleanup
#else
#define autofree_f(free_func)
#define autofree autofree_f(free)
#define AUTOPTR_DECL(type, free_func)
#define autoptr(type) type *
#endif

// -------------------- C++ -------------------- //

#ifdef __cplusplus
#define EXTERN_C_START extern "C" {
#define EXTERN_C_END   }
#else
#define EXTERN_C_START
#define EXTERN_C_END
#endif

// -------------------- LOGIC -------------------- //

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define elif else if
#define and  &&
#define or   ||

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
// Unused variable
#define UNUSED(var) (void)(var)
// Asserts that a condition is true, aborts if false.
#define ASSERT(condition)                                                                                              \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      LOG_DEBUG("ASSERTION FAILED: %s", #condition);                                                                   \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while (0)

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

// -------------------- STRINGS -------------------- //

// Stringify
#define STR(macro_or_string) #macro_or_string
// Create multiline string.
// const char *str = MULTILINE_STRING(
//                   Hello,
//                   World!
//                   );
#define MULTILINE_STRING(...) #__VA_ARGS__
// Check if strings are equal.
#define STR_EQUAL(s1, s2) (strcmp((const char *)(s1), (const char *)(s2)) == 0)
// Check if string contains substring.
#define STR_CONTAINS(s1, s2) ((s1 && s2) ? strstr((const char *)(s1), (const char *)(s2)) != NULL : false)
// Check if string contains substring (case-insensitive).
#define STR_CONTAINS_CASE(s1, s2) ((s1 && s2) ? strcasestr((const char *)(s1), (const char *)(s2)) != NULL : false)

// -------------------- UUID -------------------- //

const char *generate_uuid4();

// -------------------- STRING ARRAY -------------------- //

// NULL-terminated array of strings.
typedef char **strv;

strv strv_new(const char *first, ...);
void strv_add(strv s, const char *add);
void strv_add_strv(strv s, strv add);
size_t strv_len(strv s);
char *strv_merge(strv s, const char *separator);
void strv_free(strv *s);
AUTOPTR_DEFINE(strv, strv_free);

// -------------------- ARRAYS -------------------- //

#define STATIC_ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

// Dynamic array struct
typedef struct {
  size_t len;
  size_t capacity;
  void (*item_free_func)(void *);
  void **items;
} array;

// Create new array with `initial_capacity`
static inline array *array_new(size_t initial_capacity) {
  array *a = calloc(1, sizeof(array));
  a->capacity = initial_capacity;
  a->items = calloc(1, sizeof(void *) * initial_capacity);
  return a;
}

// Create new array struct with `initial_capacity` and `item_free_func`
static inline array *array_new_with_free_func(size_t initial_capacity, void (*item_free_func)(void *)) {
  array *a = calloc(1, sizeof(array));
  a->capacity = initial_capacity;
  a->item_free_func = item_free_func;
  a->items = calloc(1, sizeof(void *) * initial_capacity);
  return a;
}

// Free array and its items if `item_free_func` is provided
static inline void array_free(array *a) {
  if (a->item_free_func) for_range(i, 0, a->len) a->item_free_func(a->items[i]);
  if (a->items) free(a->items);
}

AUTOPTR_DEFINE(array, array_free)

// Add `item` to array
static inline void array_add(array *a, void *item) {
  if (a->capacity == a->len) {
    a->capacity *= 2;
    a->items = realloc(a->items, a->capacity * sizeof(void *));
  }
  a->items[a->len++] = item;
}

// Insert `item` at `idx` in array. If `idx` is -1 - append to end.
static inline void array_insert(array *a, int64_t idx, void *item) {
  if (idx == -1) {
    array_add(a, item);
    return;
  }
  if (a->capacity == a->len) {
    a->capacity *= 2;
    a->items = realloc(a->items, a->capacity * sizeof(void *));
  }
  for (size_t i = a->len; i > idx; i--) a->items[i] = a->items[i - 1];
  a->items[idx] = item;
  a->len++;
}

// Remove and free last item from array
static inline void array_pop(array *a) {
  if (a->item_free_func) a->item_free_func(a->items[a->len - 1]);
  a->len--;
}

// Remove last item from array and return it without freeing it.
// User is responsible for freeing the returned item.
static inline void *array_steal(array *a) {
  if (a->len == 0) return NULL;
  void *item = a->items[a->len - 1];
  a->len--;
  return item;
}

// Remove item at `idx` from array and return it without freeing it.
// Move items after idx.
// User is responsible for freeing the returned item.
static inline void *array_steal_idx(array *a, size_t idx) {
  if (a->len == 0) return NULL;
  void *item = a->items[idx];
  for (size_t i = idx; i < a->len - 1; i++) a->items[i] = a->items[i + 1];
  a->len--;
  return item;
}

// Get last item of the array or NULL if array is empty.
static inline void *array_last(array *a) { return a->len == 0 ? NULL : a->items[a->len - 1]; }

// -------------------- TEMPORARY STRING -------------------- //

// This is temporary buffer for printing formatted strings.
// If you need to get formatted string but not wanting to use malloc, use this.
// Default size is 8192 bytes. You can change it by defining TB_TMP_STR_BUFFER_SIZE.
// When buffer is full - starts overriding buffer from the beginning.
const char *tmp_str_printf(const char *format, ...);

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
static inline const char *path_base_name(const char *path) {
  const char *last_sep = strrchr(path, '/');
  if (!last_sep) last_sep = strrchr(path, '\\');
  return last_sep ? last_sep + 1 : path;
}
// Get path extension. e. g. "/home/user/file.txt" -> "txt".
static inline const char *path_ext(const char *path) {
  const char *last_dot = strrchr(path, '.');
  return last_dot ? last_dot + 1 : NULL;
}
// Get path file name. e. g. "/home/user/file.txt" -> "file".
static inline const char *path_file_name(const char *path) {
  const char *dot = strrchr(path, '.');
  if (!dot) return NULL;
  const char *base_name = path_base_name(path);
  size_t len = dot - base_name;
  char filename[len + 1];
  strncpy(filename, base_name, len);
  filename[len] = '\0';
  return tmp_str_printf("%s", filename);
}

// -------------------- TIME -------------------- //

// Get current time
#define TIME_NOW time(NULL)
// Start timer
#define TIMER_START clock_t __timer_start = clock();
// Get elapsed time in milliseconds
#define TIMER_ELAPSED_MS ((double)(clock() - __timer_start) / CLOCKS_PER_SEC)
// Generate random seed
#define RANDOM_SEED() srand((unsigned int)(TIME_NOW ^ getpid()));

// -------------------- SYSTEM COMMANDS -------------------- //

int cmd_run_stdout(const char *cmd, char **std_out);

// -------------------- COLORS -------------------- //

const char *generate_hex_as_str();

#endif // TOOLBOX_H

// -------------------------------------------------------- //
//                      IMPLEMENTATION                      //
// -------------------------------------------------------- //

#ifdef TOOLBOX_IMPLEMENTATION

// -------------------- TEMPORARY STRING -------------------- //

#ifndef TB_TMP_STR_BUFFER_SIZE
#define TB_TMP_STR_BUFFER_SIZE 8192
#endif // TB_TMP_STR_BUFFER_SIZE

static char toolbox_tmp_str[TB_TMP_STR_BUFFER_SIZE];
static size_t toolbox_tmp_str_offset = 0;

const char *tmp_str_printf(const char *format, ...) {
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

// -------------------- UUID -------------------- //

const char *generate_uuid4() {
  static thread_local char uuid[37];
  const char *hex = "0123456789abcdef";
  static thread_local int seeded = 0;
  if (!seeded) {
    srand(time(NULL) ^ (unsigned long)&seeded);
    seeded = 1;
  }
  for (int i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) uuid[i] = '-';
    else if (i == 14) uuid[i] = '4';
    else if (i == 19) {
      int r = rand() % 4;
      uuid[i] = (r == 0) ? '8' : (r == 1) ? '9' : (r == 2) ? 'a' : 'b';
    } else uuid[i] = hex[rand() % 16];
  }
  uuid[36] = '\0';
  return uuid;
}

// -------------------- STRING ARRAY -------------------- //

strv strv_new(const char *first, ...) {
  strv s = malloc(sizeof(char *));
  if (!s) return NULL;
  s[0] = NULL;
  va_list args;
  va_start(args, first);
  while (first) {
    strv_add(s, first);
    first = va_arg(args, const char *);
  }
  va_end(args);
  return s;
}

void strv_add(strv s, const char *add) {
  if (!s) return;
  size_t len = strv_len(s);
  s = realloc(s, (len + 2) * sizeof(char *));
  if (!s) return;
  s[len] = strdup(add);
  s[len + 1] = NULL;
}

void strv_add_strv(strv s, strv add) {
  if (!s || !add) return;
  size_t len = strv_len(s);
  size_t add_len = strv_len(add);
  s = realloc(s, (len + add_len + 1) * sizeof(char *));
  if (!s) return;
  for (size_t i = 0; i < add_len; i++) { s[len + i] = strdup(add[i]); }
  s[len + add_len] = NULL;
}

size_t strv_len(strv s) {
  size_t len = 0;
  for (size_t i = 0; s[i]; i++) len++;
  return len;
}

char *strv_merge(strv s, const char *separator) {
  if (!s || !separator) return NULL;
  size_t len = 0, sep_len = 0;
  for (size_t i = 0; s[i]; i++) {
    len += strlen(s[i]);
    if (s[i + 1]) len += sep_len;
  }
  char *merged = malloc(len + 1);
  if (!merged) return NULL;
  merged[0] = '\0';
  for (size_t i = 0; s[i]; i++) {
    strcat(merged, s[i]);
    if (s[i + 1]) strcat(merged, separator);
  }
  return merged;
}

void strv_free(strv *s) {
  if (!s || !*s) return;
  for (size_t i = 0; (*s)[i]; i++) free((*s)[i]);
  free(*s);
  *s = NULL;
}

// -------------------- COLORS -------------------- //

const char *generate_hex_as_str() {
  static char hex[8] = {0};
  sprintf(hex, "#%06x", rand() % 0xFFFFFF);
  return hex;
}

// -------------------- SYSTEM COMMANDS -------------------- //

int cmd_run_stdout(const char *cmd, char **std_out) {
  if (!cmd || !std_out) return -1;
  *std_out = NULL;
  FILE *fp = popen(cmd, "r");
  if (!fp) return -1;
  size_t cap = 4096;
  size_t len = 0;
  char *buf = malloc(cap);
  if (!buf) {
    pclose(fp);
    return -1;
  }
  // Read in chunks
  while (1) {
    // Ensure we have space for at least 1 more byte plus null terminator
    if (cap - len < 2) {
      size_t newcap = cap * 2;
      char *tmp = realloc(buf, newcap);
      if (!tmp) {
        free(buf);
        pclose(fp);
        return -1;
      }
      buf = tmp;
      cap = newcap;
    }
    size_t to_read = cap - len - 1; // Leave room for null terminator
    size_t nread = fread(buf + len, 1, to_read, fp);
    if (nread == 0) break;
    len += nread;
    if (ferror(fp)) {
      free(buf);
      pclose(fp);
      return -1;
    }
  }
  buf[len] = '\0';
  int status = pclose(fp);
  if (status == -1) {
    free(buf);
    return -1;
  }
  // Convert wait status to exit code
#ifdef WIFEXITED
  if (WIFEXITED(status)) {
    status = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    // Command terminated by signal, return negative signal number
    status = -WTERMSIG(status);
  }
  // else keep the raw status
#endif
  // Shrink buffer to actual size if significantly smaller
  if (len + 1 < cap / 2) {
    char *tmp = realloc(buf, len + 1);
    if (tmp) buf = tmp;
    // If realloc fails, we keep the original buffer (not a critical error)
  }
  *std_out = buf;
  return status;
}

#endif // TOOLBOX_IMPLEMENTATION
