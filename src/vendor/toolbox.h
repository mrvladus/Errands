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

// ---------- TEMPORARY STRING ---------- //

// This is temporary buffer for printing formatted strings.
// Used by printing NULL-terminated string to buffer and move offset.
// When buffer is full - start override buffer from the beginning.

#define TB_TMP_STR_BUFFER_SIZE 4096
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

#endif // TOOLBOX_H
