#ifndef TOOLBOX_H
#define TOOLBOX_H

#ifndef TOOLBOX_API
#define TOOLBOX_API static inline
#endif // TOOLBOX_API

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// -------------------- MACROS -------------------- //

// #define LOG_FMT(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)
// #define LOG(msg) LOG_FMT("%s", msg)

/*

Debug messages macros.
Enabled by default.
To disable debug messages put '#define TB_DISABLE_DEBUG' before including 'toolbox.h'

*/

// Print formatted message with filename, line number and function name.
#define DEBUG_LOG_FMT(format, ...)                                                                                     \
  fprintf(stderr, "%s:%d:%s: " format "\n", tb_path_base_name(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__)
// Print unformatted debug message
#define DEBUG_LOG(msg) DEBUG_LOG_FMT("%s", msg)

// Dummy macros when debug is disabled.
#ifdef TB_DISABLE_DEBUG
#define DEBUG_LOG_FMT(format, ...)
#define DEBUG_LOG(msg)
#endif // TB_DISABLE_DEBUG

// The best debug method. Improved. :)
#define HERE DEBUG_LOG("HERE")
// Formatted message version of HERE macro.
#define HERE_FMT(format, ...) DEBUG_LOG_FMT("HERE: " format, ##__VA_ARGS__)

// Print TODO formatted message.
#define TODO(format, ...) DEBUG_LOG_FMT("TODO: " format, ##__VA_ARGS__)

// Prints 'UNIMPLEMENTED' message and exits with error code 1.
#define UNIMPLEMENTED                                                                                                  \
  do {                                                                                                                 \
    DEBUG_LOG("UNIMPLEMENTED");                                                                                        \
    exit(1);                                                                                                           \
  } while (0)
// Formatted message version of UNIMPLEMENTED macro.
#define UNIMPLEMENTED_FMT(format, ...)                                                                                 \
  do {                                                                                                                 \
    DEBUG_LOG_FMT("UNIMPLEMENTED: " format, ##__VA_ARGS__);                                                            \
    exit(1);                                                                                                           \
  } while (0)

// Print PANIC and exit with error code 1.
#define PANIC                                                                                                          \
  do {                                                                                                                 \
    DEBUG_LOG("PANIC");                                                                                                \
    exit(1);                                                                                                           \
  } while (0)
// Formatted message version of PANIC macro.
#define PANIC_FMT(format, ...)                                                                                         \
  do {                                                                                                                 \
    DEBUG_LOG_FMT("PANIC: " format, ##__VA_ARGS__);                                                                    \
    exit(1);                                                                                                           \
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
#define PROFILE_FUNC_START clock_t __start_time = clock()
// Stop profile timer and write time of execution to stderr.
#define PROFILE_FUNC_END DEBUG_LOG_FMT("%f sec.", (double)(clock() - __start_time) / CLOCKS_PER_SEC)

// Profile block of code
#define PROFILE_BLOCK(code_block)                                                                                      \
  do {                                                                                                                 \
    PROFILE_FUNC_START;                                                                                                \
    code_block;                                                                                                        \
    PROFILE_FUNC_END;                                                                                                  \
  } while (0)

#else // Dummy macros when profiling is disabled.
#define PROFILE_FUNC_START
#define PROFILE_FUNC_END
#define PROFILE_BLOCK(code_block) code_block
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

static char tb_tmp_str_buffer[TB_TMP_STR_BUFFER_SIZE];
static size_t tb_tmp_str_offset = 0;

TOOLBOX_API const char *tb_tmp_str_printf(const char *format, ...) {
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
  if (len >= TB_TMP_STR_BUFFER_SIZE - tb_tmp_str_offset - 1) tb_tmp_str_offset = 0;
  // Print the string
  vsnprintf(tb_tmp_str_buffer + tb_tmp_str_offset, TB_TMP_STR_BUFFER_SIZE - tb_tmp_str_offset, format, args);
  const char *result = tb_tmp_str_buffer + tb_tmp_str_offset;
  tb_tmp_str_offset += len + 1;
  va_end(args);
  return result;
}

// -------------------- PATH FUNCTIONS -------------------- //

TOOLBOX_API const char *tb_path_base_name(const char *path) {
  const char *last_sep = strrchr(path, '/');
  if (!last_sep) last_sep = strrchr(path, '\\');
  return last_sep ? last_sep + 1 : path;
}

#endif // TOOLBOX_H
