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

// The best debug method. Improved. :)
#define TB_HERE TB_LOG_DEBUG("HERE")

// Print TODO formatted message.
#define TB_TODO(format, ...) TB_LOG_DEBUG("TODO: " format, ##__VA_ARGS__)

// Prints 'UNIMPLEMENTED' message and exits with error code 1.
#define TB_UNIMPLEMENTED                                                                                               \
  do {                                                                                                                 \
    TB_LOG_DEBUG("UNIMPLEMENTED");                                                                                     \
    exit(EXIT_FAILURE);                                                                                                \
  } while (0)

// Print PANIC and exit with error code 1.
#define TB_PANIC                                                                                                       \
  do {                                                                                                                 \
    LOG_DEBUG("PANIC");                                                                                                \
    exit(EXIT_FAILURE);                                                                                                \
  } while (0)

/*

Profiling macros.
Disabled by default.
To enable profiling put '#define TB_ENABLE_PROFILING' before including 'toolbox.h'

Usage:

Using PROFILE_FUNC_START and PROFILE_FUNC_END macros:

void func() {
    PROFILE_FUNC_START;
    do_stuff();
    PROFILE_FUNC_END;
}

Using PROFILE_BLOCK macro:

void func() {
    PROFILE_BLOCK({
        do_stuff();
    })
}

*/

#ifdef TB_ENABLE_PROFILING

// Start profile timer.
#define TB_PROFILE_FUNC_START clock_t __start_time = clock()
// Stop profile timer and write time of execution to stderr.
#define TB_PROFILE_FUNC_END TB_LOG_DEBUG("%f sec.", (double)(clock() - __start_time) / CLOCKS_PER_SEC)

// Profile block of code
#define TB_PROFILE_BLOCK(code_block)                                                                                   \
  do {                                                                                                                 \
    TB_PROFILE_FUNC_START;                                                                                             \
    code_block;                                                                                                        \
    TB_PROFILE_FUNC_END;                                                                                               \
  } while (0)

#else // Dummy macros when profiling is disabled.
#define TB_PROFILE_FUNC_START
#define TB_PROFILE_FUNC_END
#define TB_PROFILE_BLOCK(code_block) code_block
#endif // TB_ENABLE_PROFILING

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

bool tb_str_contains(const char *str, const char *sub_str);

// -------------------- TIME FUNCTIONS -------------------- //

time_t tb_time_now();

#endif // TOOLBOX_H

#ifdef TOOLBOX_IMPLEMENTATION

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

// -------------------- TIME FUNCTIONS -------------------- //

time_t tb_time_now() { return time(NULL); }

#endif // TOOLBOX_IMPLEMENTATION
