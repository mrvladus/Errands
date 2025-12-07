/*

MIT License

Copyright (c) 2025 Vlad Krupinskii <mrvladus@yandex.ru>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

------------------------------------------------------------------------------

json.h - Header-only C/C++ library to parse and serialize JSON.

------------------------------------------------------------------------------

Features:

- Parsing and Serialization in ONE library
- Fast
- Simple API
- Supports trailing commas (like it's should be)

------------------------------------------------------------------------------

Usage:

Define JSON_H_IMPLEMENTATION in ONE file before including "json.h" to pull functions implementations.
In other files just include "json.h".

#define JSON_H_IMPLEMENTATION
#include "json.h"

------------------------------------------------------------------------------

*/

#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ------------------------------------------------------------------------- //

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// ------------------------------------------------------------------------- //

// Define your own malloc and free functions before including json.h if you need to.
// Othetwise it will use stdlib malloc/free.

// #define JSON_H_MALLOC my_custom_malloc
// #define JSON_H_FREE my_custom_free
// #include "json.h"

#if !defined(JSON_H_MALLOC) && !defined(JSON_H_CALLOC) && !defined(JSON_H_FREE)
#include <stdlib.h>
#define JSON_H_MALLOC malloc
#define JSON_H_CALLOC calloc
#define JSON_H_FREE   free
#endif

// ------------------------------------------------------------------------- //

// JSON object type.
typedef enum {
  JSON_TYPE_NULL,
  JSON_TYPE_BOOL,
  JSON_TYPE_INT,
  JSON_TYPE_DOUBLE,
  JSON_TYPE_STRING,
  JSON_TYPE_ARRAY,
  JSON_TYPE_OBJECT,
} JSONType;

// JSON object struct.
typedef struct JSON JSON;
struct JSON {
  // Type of object. Read-only.
  JSONType type;
  // Key of the object. Set only for children of JSON_TYPE_OBJECT.
  // Don't change directly - use json_replace_string().
  char *key;
  // Parent JSON_TYPE_OBJECT or JSON_TYPE_ARRAY.
  JSON *parent;
  // First child of linked list of JSON_TYPE_OBJECT or JSON_TYPE_ARRAY children.
  JSON *child;
  // Next sibling in the linked list. NULL for last child of JSON_TYPE_OBJECT or JSON_TYPE_ARRAY.
  JSON *next;
  // Value of the object. Not set for JSON_TYPE_OBJECT, JSON_TYPE_ARRAY and JSON_TYPE_NULL.
  union {
    // Boolean value. Set for JSON_TYPE_BOOL.
    bool bool_val;
    // Integer value. Set for JSON_TYPE_INT.
    int64_t int_val;
    // Double value. Set for JSON_TYPE_DOUBLE.
    double double_val;
    // String value. Set for JSON_TYPE_STRING.
    // Don't change directly - use json_replace_string().
    char *string_val;
  };
};

// ---------- PARSING ---------- //

// Parse JSON string. Returns NULL on error.
JSON *json_parse(const char *json);

// ---------- PRINTING ---------- //

// Print JSON node and it's children nodes recursively as JSON string.
// Returned string must be freed by the caller.
char *json_print(JSON *node);

// ---------- SERIALIZATION ---------- //

// Create new JSON_TYPE_OBJECT.
JSON *json_object_new();
// Add child to object at key. If key exists - replace it.
bool json_object_add(JSON *object, const char *key, JSON *child);
// Remove child with key from object.
bool json_object_remove(JSON *object, const char *key);
// Get child with key from object.
JSON *json_object_get(JSON *object, const char *key);

// Create new JSON_TYPE_ARRAY.
JSON *json_array_new();
// Add child to array.
bool json_array_add(JSON *array, JSON *child);
// Remove child from array.
bool json_array_remove(JSON *array, JSON *child);

// Create new JSON_TYPE_NULL.
JSON *json_null_new();
// Create new JSON_TYPE_BOOL.
JSON *json_bool_new(bool value);
// Create new JSON_TYPE_INT.
JSON *json_int_new(int64_t value);
// Create new JSON_TYPE_DOUBLE.
JSON *json_double_new(double value);
// Create new JSON_TYPE_STRING.
JSON *json_string_new(const char *value);

// Deep copy JSON node.
JSON *json_dup(JSON *node);

// ---------- CLEANUP ---------- //

// Free JSON object and all its children.
void json_free(JSON *object);

// ---------- UTILS ---------- //

// Read file from path to allocated string.
char *json_read_file_to_string(const char *path);
// Replace allocated string. Use for changing JSON object key and string value.
void json_replace_string(char **old_string, const char *new_string);

// ------------------------------------------------------------------------- //

#ifdef __cplusplus
}
#endif // __cplusplus

// ------------------------------------------------------------------------- //

#endif // JSON_H

// ------------------------------------------------------------------------- //

#ifdef JSON_H_IMPLEMENTATION

#ifdef JSON_H_DEBUG
#define JSON_LOG_DEBUG(fmt, ...) fprintf(stderr, "[JSON.H DEBUG] " fmt "\n", ##__VA_ARGS__);
#else
#define JSON_LOG_DEBUG(fmt, ...)
#endif

// ------------------------------------------------------------------------- //

// Private function.
// Get length of string with escaped characters.
static size_t json__escaped_strlen(const char *str) {
  if (!str) return 0;
  size_t len = 0;
  const char *p = str;
  while (*p) {
    switch (*p) {
    case '"':
    case '\\':
    case '\b':
    case '\f':
    case '\n':
    case '\r':
    case '\t': len += 2; break;
    default:
      if ((unsigned char)*p < 0x20) len += 6;
      else len += 1;
      break;
    }
    p++;
  }
  return len;
}

// Private function.
// Escapes a JSON string and returns a newly allocated string
// Caller must free the returned string
static char *json__escape(const char *str) {
  if (str == NULL) return NULL;
  size_t len = json__escaped_strlen(str);
  char *result = JSON_H_MALLOC(len + 1);
  if (result == NULL) return NULL;
  const char *src = str;
  char *dst = result;
  while (*src) {
    switch (*src) {
    case '"':
      *dst++ = '\\';
      *dst++ = '"';
      break;
    case '\\':
      *dst++ = '\\';
      *dst++ = '\\';
      break;
    case '\b':
      *dst++ = '\\';
      *dst++ = 'b';
      break;
    case '\f':
      *dst++ = '\\';
      *dst++ = 'f';
      break;
    case '\n':
      *dst++ = '\\';
      *dst++ = 'n';
      break;
    case '\r':
      *dst++ = '\\';
      *dst++ = 'r';
      break;
    case '\t':
      *dst++ = '\\';
      *dst++ = 't';
      break;
    default:
      if ((unsigned char)*src < 0x20) {
        // Control character - escape as \u00XX
        snprintf(dst, 7, "\\u%04X", (unsigned char)*src);
        dst += 6;
      } else *dst++ = *src;
      break;
    }
    src++;
  }
  *dst = '\0';
  return result;
}

// Private function.
// Unescapes a JSON string in-place.
static void json__unescape(char *str) {
  if (str == NULL) return;
  char *src = str;
  char *dst = str;
  while (*src) {
    if (*src == '\\') {
      src++; // Skip the backslash
      switch (*src) {
      case '"': *dst = '"'; break;
      case '\\': *dst = '\\'; break;
      case '/': *dst = '/'; break;
      case 'b': *dst = '\b'; break;
      case 'f': *dst = '\f'; break;
      case 'n': *dst = '\n'; break;
      case 'r': *dst = '\r'; break;
      case 't': *dst = '\t'; break;
      case 'u': {
        *dst++ = '\\';
        *dst = 'u';
        for (uint8_t i = 0; i < 4 && src[1]; i++) *++dst = *++src;
        break;
      }
      default: *dst = *src; break;
      }
      src++;
      dst++;
    } else *dst++ = *src++;
  }
  *dst = '\0';
}

// Private function.
// Check if character is whitespace.
static bool json_isspace(char c) {
  switch (c) {
  case ' ':
  case '\t':
  case '\n':
  case '\r': return true;
  default: return false;
  }
}

// Private function.
// strdup() with JSON_H_MALLOC.
static char *json__strdup(const char *str) {
  size_t len = strlen(str);
  char *dup = JSON_H_MALLOC(len + 1);
  if (!dup) return NULL;
  memcpy(dup, str, len + 1);
  return dup;
}

// Private function.
// strndup() with JSON_H_MALLOC.
static char *json__strndup(const char *str, size_t n) {
  if (str == NULL) return NULL;
  size_t len = strlen(str);
  size_t copy_len = (n < len) ? n : len;
  char *dup = JSON_H_MALLOC(copy_len + 1);
  if (!dup) return NULL;
  memcpy(dup, str, copy_len);
  dup[copy_len] = '\0';
  return dup;
}

// ------------------------------------------------------------------------- //

static JSON *json__parse_node(const char *json, size_t *idx);
static void json__parse_value(const char *json, size_t *idx, JSON *node);

// Private function.
static void json__skip_whitespace(const char *json, size_t *idx) {
  while (json_isspace(json[*idx])) (*idx)++;
}

// Private function.
static bool json__parse_key(const char *json, size_t *idx, JSON *node) {
  json__skip_whitespace(json, idx);
  // It's a value
  if (json[*idx] != '"') {
    node->key = NULL;
    return false;
  }
  (*idx)++; // Skip '"'
  size_t start = *idx;
  while (json[*idx] != '"') (*idx)++;
  node->key = json__strndup(json + start, *idx - start);
  (*idx)++; // Skip '"'
  JSON_LOG_DEBUG("Parsed key: \"%s\"", node->key);
  return true;
}

// Private function.
static void json__parse_null_value(const char *json, size_t *idx, JSON *node) {
  node->type = JSON_TYPE_NULL;
  (*idx) += 4;
  JSON_LOG_DEBUG("Parsed NULL");
}

// Private function.
static void json__parse_bool_value(const char *json, size_t *idx, JSON *node) {
  if (json[*idx] == 't') {
    node->bool_val = true;
    node->type = JSON_TYPE_BOOL;
    (*idx) += 4;
    JSON_LOG_DEBUG("Parsed BOOL: true");
  } else if (json[*idx] == 'f') {
    node->bool_val = false;
    node->type = JSON_TYPE_BOOL;
    (*idx) += 5;
    JSON_LOG_DEBUG("Parsed BOOL: false");
  }
}

// Private function.
static void json__parse_number_value(const char *json, size_t *idx, JSON *node) {
  bool is_double = false;
  size_t tmp = *idx;
  while (!(json_isspace(json[tmp]) || json[tmp] == ',' || json[tmp] == '}' || json[tmp] == ']')) {
    if (json[tmp] == '.') is_double = true;
    tmp++;
  }
  if (is_double) {
    node->double_val = strtod(json + (*idx), NULL);
    node->type = JSON_TYPE_DOUBLE;
    JSON_LOG_DEBUG("Parsed DOUBLE: %f", node->double_val);
  } else {
    node->int_val = strtol(json + (*idx), NULL, 10);
    node->type = JSON_TYPE_INT;
    JSON_LOG_DEBUG("Parsed INT: %ld", node->int_val);
  }
  (*idx) = tmp;
}

// Private function.
static void json__parse_string_value(const char *json, size_t *idx, JSON *node) {
  node->type = JSON_TYPE_STRING;
  (*idx)++; // Skip '"'
  size_t start = *idx;
  while (json[*idx] != '"') {
    if (json[*idx] == '\\') (*idx)++; // Skip over escape sequences
    (*idx)++;
  }
  char *val = json__strndup(json + start, *idx - start);
  json__unescape(val);
  node->string_val = val;
  (*idx)++; // Skip '"'
  JSON_LOG_DEBUG("Parsed STRING: \"%s\"", node->string_val);
}

// Private function.
static void json__parse_array_value(const char *json, size_t *idx, JSON *node) {
  JSON_LOG_DEBUG("Parse array: [");
  node->type = JSON_TYPE_ARRAY;
  (*idx)++; // Skip '['
  JSON *last_child = NULL;
  while (json[*idx] != ']') {
    json__skip_whitespace(json, idx);
    JSON *child = JSON_H_CALLOC(1, sizeof(JSON));
    json__parse_value(json, idx, child);
    child->parent = node;
    json__skip_whitespace(json, idx);
    if (!last_child) {
      node->child = child;
      last_child = child;
    } else {
      last_child->next = child;
      last_child = child;
    }
    if (json[*idx] == ',') (*idx)++; // Skip ','
  }
  (*idx)++; // Skip ']'
  JSON_LOG_DEBUG("]");
}

// Private function.
static void json__parse_object_value(const char *json, size_t *idx, JSON *node) {
  JSON_LOG_DEBUG("Parse object: {");
  node->type = JSON_TYPE_OBJECT;
  (*idx)++; // Skip '{'
  json__skip_whitespace(json, idx);
  JSON *last_child = NULL;
  while (json[*idx] != '}') {
    JSON *child = json__parse_node(json, idx);
    child->parent = node;
    if (!last_child) {
      node->child = child;
      last_child = child;
    } else {
      last_child->next = child;
      last_child = child;
    }
    if (json[*idx] == ',') (*idx)++; // Skip ','
  }
  (*idx)++; // Skip '}'
  JSON_LOG_DEBUG("}");
}

// Private function.
static void json__parse_value(const char *json, size_t *idx, JSON *node) {
  switch (json[*idx]) {
  case 'n': json__parse_null_value(json, idx, node); break;
  case 't':
  case 'f': json__parse_bool_value(json, idx, node); break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case '+':
  case '-': json__parse_number_value(json, idx, node); break;
  case '"': json__parse_string_value(json, idx, node); break;
  case '[': json__parse_array_value(json, idx, node); break;
  case '{': json__parse_object_value(json, idx, node); break;
  default: fprintf(stderr, "Invalid character: %c\n", json[*idx]); exit(1);
  }
}

// Private function.
static JSON *json__parse_node(const char *json, size_t *idx) {
  JSON *node = JSON_H_CALLOC(1, sizeof(JSON));
  if (json__parse_key(json, idx, node)) {
    json__skip_whitespace(json, idx);
    if (json[*idx] == ':') {
      (*idx)++; // Skip ':'
      json__skip_whitespace(json, idx);
    }
  }
  json__parse_value(json, idx, node);
  json__skip_whitespace(json, idx);
  return node;
}

JSON *json_parse(const char *json) {
  JSON *root = JSON_H_CALLOC(1, sizeof(JSON));
  root->type = JSON_TYPE_OBJECT;
  size_t idx = 0;
  json__skip_whitespace(json, &idx);
  json__parse_value(json, &idx, root);
  return root;
}

// ------------------------------------------------------------------------- //

// Private function.
// Print JSON node to string recursively.
static size_t json__print_node(JSON *node, char *str, size_t offset) {
  if (node->key) offset += sprintf(str + offset, "\"%s\":", node->key);
  switch (node->type) {
  case JSON_TYPE_NULL: offset += sprintf(str + offset, "null"); break;
  case JSON_TYPE_BOOL: offset += sprintf(str + offset, "%s", node->bool_val ? "true" : "false"); break;
  case JSON_TYPE_INT: offset += sprintf(str + offset, "%ld", node->int_val); break;
  case JSON_TYPE_DOUBLE: offset += sprintf(str + offset, "%f", node->double_val); break;
  case JSON_TYPE_STRING: {
    char *escaped_string = json__escape(node->string_val);
    offset += sprintf(str + offset, "\"%s\"", escaped_string);
    JSON_H_FREE(escaped_string);
    break;
  }
  case JSON_TYPE_ARRAY:
  case JSON_TYPE_OBJECT: {
    offset += sprintf(str + offset, node->type == JSON_TYPE_ARRAY ? "[" : "{");
    for (JSON *child = node->child; child; child = child->next) {
      offset = json__print_node(child, str, offset);
      if (child->next) offset += sprintf(str + offset, ",");
    }
    offset += sprintf(str + offset, node->type == JSON_TYPE_ARRAY ? "]" : "}");
    break;
  }
  }
  return offset;
}

char *json_print(JSON *node) {
  if (!node) return NULL;
  // Get string len for allocation
  size_t len = 0;
  JSON *curr = node;
  while (curr) {
    if (curr->key) len += json__escaped_strlen(curr->key) + 3; // quotes + colon
    switch (curr->type) {
    case JSON_TYPE_NULL: len += 4; break;                      // null
    case JSON_TYPE_BOOL: len += curr->bool_val ? 4 : 5; break; // true or false
    case JSON_TYPE_INT: len += snprintf(NULL, 0, "%ld", curr->int_val); break;
    case JSON_TYPE_DOUBLE: len += snprintf(NULL, 0, "%f", curr->double_val); break;
    case JSON_TYPE_STRING: len += json__escaped_strlen(curr->string_val) + 2; break; // quotes
    case JSON_TYPE_ARRAY:
    case JSON_TYPE_OBJECT: {
      len += 2; // [ ] or { }
      size_t child_count = 0;
      for (JSON *child = curr->child; child; child = child->next) child_count++;
      if (child_count > 0) {
        len += child_count - 1; // commas
        curr = curr->child;
        continue;
      }
    }
    }
    while (curr && !curr->next) { curr = curr->parent; }
    if (curr) { curr = curr->next; }
  }
  if (len == 0) return NULL;
  char *out = JSON_H_MALLOC(len + 1);
  if (!out) return NULL;
  json__print_node(node, out, 0);
  return out;
}

// ------------------------------------------------------------------------- //

JSON *json_object_new() {
  JSON *node = JSON_H_CALLOC(1, sizeof(JSON));
  if (!node) return NULL;
  node->type = JSON_TYPE_OBJECT;
  return node;
}

bool json_object_add(JSON *object, const char *key, JSON *child) {
  if (!object || object->type != JSON_TYPE_OBJECT || !key || !child) return false;
  child->parent = object;
  json_replace_string(&child->key, key);
  if (!object->child) {
    object->child = child;
    return true;
  }
  JSON *curr = object->child;
  JSON *prev = NULL;
  while (curr) {
    if (!strcmp(curr->key, key)) {
      child->next = curr->next;
      if (!prev) object->child = child; // First child
      else prev->next = child;
      json_free(curr);
      return true;
    }
    if (curr->next) {
      prev = curr;
      curr = curr->next;
    } else break;
  }
  curr->next = child;
  return true;
}

bool json_object_remove(JSON *object, const char *key) {
  if (!object || !key || !(object->type == JSON_TYPE_OBJECT)) return false;
  JSON *curr = object->child;
  JSON *prev = NULL;
  while (curr) {
    if (!strcmp(key, curr->key)) {
      JSON *next = curr->next;
      if (!prev) object->child = next;
      else prev->next = next;
      json_free(curr);
      return true;
    }
    prev = curr;
    curr = curr->next;
  }
  return false;
}

JSON *json_object_get(JSON *object, const char *key) {
  if (!object || !key || object->type != JSON_TYPE_OBJECT) return NULL;
  for (JSON *child = object->child; child; child = child->next)
    if (!strcmp(key, child->key)) return child;
  return NULL;
}

JSON *json_dup(JSON *node) {
  JSON *new_node = JSON_H_CALLOC(1, sizeof(JSON));
  if (!new_node) return NULL;
  new_node->type = node->type;
  new_node->key = node->key ? json__strdup(node->key) : NULL;
  new_node->child = node->child ? json_dup(node->child) : NULL;
  switch (node->type) {
  case JSON_TYPE_BOOL: new_node->bool_val = node->bool_val; break;
  case JSON_TYPE_INT: new_node->int_val = node->int_val; break;
  case JSON_TYPE_DOUBLE: new_node->double_val = node->double_val; break;
  case JSON_TYPE_STRING: new_node->string_val = json__strdup(node->string_val); break;
  case JSON_TYPE_NULL:
  case JSON_TYPE_ARRAY:
  case JSON_TYPE_OBJECT: break;
  }
  return new_node;
}

JSON *json_array_new() {
  JSON *node = JSON_H_CALLOC(1, sizeof(JSON));
  if (!node) return NULL;
  node->type = JSON_TYPE_ARRAY;
  return node;
}

bool json_array_add(JSON *array, JSON *child) {
  if (!array || array->type != JSON_TYPE_ARRAY) return false;
  for (JSON *curr = array->child; curr; curr = curr->next)
    if (curr == child) return false;
  if (child->key) {
    JSON_H_FREE(child->key);
    child->key = NULL;
  }
  if (!array->child) {
    array->child = child;
    return true;
  }
  JSON *curr = array->child;
  while (curr->next)
    if (curr->next) curr = curr->next;
  curr->next = child;
  return true;
}

bool json_array_remove(JSON *array, JSON *child) {
  if (!array || !child) return false;
  JSON *curr = array->child;
  JSON *prev = NULL;
  while (curr) {
    if (child == curr) {
      if (!prev) array->child = curr->next;
      else prev->next = curr->next;
      json_free(curr);
      return true;
    }
    prev = curr;
    curr = curr->next;
  }
  return false;
}

JSON *json_null_new() {
  JSON *node = JSON_H_CALLOC(1, sizeof(JSON));
  if (!node) return NULL;
  node->type = JSON_TYPE_NULL;
  return node;
}

JSON *json_bool_new(bool value) {
  JSON *node = JSON_H_CALLOC(1, sizeof(JSON));
  if (!node) return NULL;
  node->type = JSON_TYPE_BOOL;
  node->bool_val = value;
  return node;
}

JSON *json_int_new(int64_t value) {
  JSON *node = JSON_H_CALLOC(1, sizeof(JSON));
  if (!node) return NULL;
  node->type = JSON_TYPE_INT;
  node->int_val = value;
  return node;
}

JSON *json_double_new(double value) {
  JSON *node = JSON_H_CALLOC(1, sizeof(JSON));
  if (!node) return NULL;
  node->type = JSON_TYPE_DOUBLE;
  node->double_val = value;
  return node;
}

JSON *json_string_new(const char *value) {
  JSON *node = JSON_H_CALLOC(1, sizeof(JSON));
  if (!node) return NULL;
  node->type = JSON_TYPE_STRING;
  node->string_val = json__strdup(value);
  return node;
}

// ------------------------------------------------------------------------- //

void json_free(JSON *node) {
  if (!node) return;
  JSON_LOG_DEBUG("Node free: %p", node);
  if (!node->key) {
    JSON_H_FREE(node->key);
    node->key = NULL;
  }
  if (node->type == JSON_TYPE_STRING && node->string_val) JSON_H_FREE(node->string_val);
  for (JSON *child = node->child; child; child = child->next) json_free(child);
  JSON_H_FREE(node);
}

// ------------------------------------------------------------------------- //

char *json_read_file_to_string(const char *path) {
  FILE *file = fopen(path, "r");
  if (!file) return NULL;
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *buf = JSON_H_MALLOC(file_size + 1);
  fread(buf, 1, file_size, file);
  buf[file_size] = '\0';
  fclose(file);
  return buf;
}

void json_replace_string(char **old_string, const char *new_string) {
  const char *str_to_copy = (new_string != NULL) ? new_string : ""; // Check if new_str is NULL (treat as empty string)
  char *new_copy = json__strdup(str_to_copy);
  if (new_copy == NULL) return;
  if (*old_string != NULL) JSON_H_FREE(*old_string);
  *old_string = new_copy;
}

#endif // JSON_H_IMPLEMENTATION
