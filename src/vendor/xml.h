/*

MIT License

Copyright (c) 2024-2025 Vlad Krupinskii <mrvladus@yandex.ru>

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

xml.h - Header-only C/C++ library to parse XML.

------------------------------------------------------------------------------

Features:

- One tiny header file (~400 lines of code)
- Parses regular (`<tag>Tag Text</tag>`) and self-closing tags (`<tag/>`)
- Parses tags attributes (`<tag attribute="value" />`)
- Ignores comments and processing instructions

------------------------------------------------------------------------------

Usage:

Define XML_H_IMPLEMENTATION in ONE file before including "xml.h" to include functions implementations.
In other files just include "xml.h".

#define XML_H_IMPLEMENTATION
#include "xml.h"

------------------------------------------------------------------------------

*/

#ifndef XML_H
#define XML_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifndef XML_H_API
#define XML_H_API extern
#endif // XML_H_API

// ---------- XMLString ---------- //

// NULL-terminated dynamically-growing string.
typedef struct {
  size_t len;  // Length of the string.
  size_t size; // Capacity of the string in bytes.
  char *str;   // Pointer to the null-terminated string.
} XMLString;

// Create a new `XMLString`.
// Returns `NULL` for error.
// Free with `xml_string_free()`.
XML_H_API XMLString *xml_string_new();
// Append string to the end of the `XMLString`.
XML_H_API void xml_string_append(XMLString *str, const char *append);
// Free `XMLString`.
XML_H_API void xml_string_free(XMLString *str);

// ---------- XMLList ---------- //

// Simple dynamic list that only can grow in size.
// Used in XMLNode for list of children and list of tag attributes.
typedef struct {
  size_t len;  // Length of the list.
  size_t size; // Size of the list in bytes.
  void **data; // List of pointers to list items.
} XMLList;

// Create new dynamic array
XML_H_API XMLList *xml_list_new();
// Add element to the end of the array. Grow if needed.
XML_H_API void xml_list_add(XMLList *list, void *data);

// ---------- XMLNode ---------- //

// Tags attribute containing key and value.
// Like: <tag foo_key="foo_value" bar_key="bar_value" />
typedef struct XMLAttr {
  char *key;
  char *value;
} XMLAttr;

// The main object to interact with parsed XML nodes. Represents single XML tag.
typedef struct XMLNode XMLNode;
struct XMLNode {
  char *tag;         // Tag string.
  char *text;        // Inner text of the tag. NULL if tag has no inner text.
  XMLList *attrs;    // List of tag attributes. Check "node->attrs->len" if it has items.
  XMLNode *parent;   // Node's parent node. NULL for the root node.
  XMLList *children; // List of tag's sub-tags. Check "node->children->len" if it has items.
};

// Create new `XMLNode`.
// `parent` can be NULL if node is root. If parent is not NULL, node will be added to parent's children list.
// `tag` and `inner_text` can be NULL.
// Returns `NULL` for error.
// Free with `xml_node_free()`.
XML_H_API XMLNode *xml_node_new(XMLNode *parent, const char *tag, const char *inner_text);
// Parse XML string and return root XMLNode.
// Returns NULL for error.
// Free with `xml_node_free()`.
XML_H_API XMLNode *xml_parse_string(const char *xml);
// Parse XML file for given path and return root XMLNode.
// Returns NULL for error.
// Free with `xml_node_free()`.
XML_H_API XMLNode *xml_parse_file(const char *path);
// Get child of the node at index.
// Returns NULL if not found.
XML_H_API XMLNode *xml_node_child_at(XMLNode *node, size_t idx);
// Find xml tag by path like "div/p/href"
// If exact is "false" - will search for tags which name contains substring "tag"
XML_H_API XMLNode *xml_node_find_by_path(XMLNode *root, const char *path, bool exact);
// Get first matching tag in the tree.
// If exact is "false" - will return first tag which name contains substring "tag"
XML_H_API XMLNode *xml_node_find_tag(XMLNode *node, const char *tag, bool exact);
// Get value of the tag attribute.
// Returns NULL if not found.
XML_H_API const char *xml_node_attr(XMLNode *node, const char *attr_key);
// Add attribute with `key` and `value` to the node's list of attributes
XML_H_API void xml_node_add_attr(XMLNode *node, const char *key, const char *value);
// Serialize `XMLNode` into `XMLString`.
XML_H_API void xml_node_serialize(XMLNode *node, XMLString *str);
// Cleanup node and all it's children recursively.
XML_H_API void xml_node_free(XMLNode *node);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // XML_H

// -------------------- FUNCTIONS IMPLEMENTATION -------------------- //

#ifdef XML_H_IMPLEMENTATION

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XML_FREE(ptr)                                                                                                  \
  if (ptr) {                                                                                                           \
    free(ptr);                                                                                                         \
    ptr = NULL;                                                                                                        \
  }

#define SKIP_WHITESPACE(xml_ptr, idx_ptr)                                                                              \
  while (isspace(xml_ptr[*idx_ptr])) (*idx_ptr)++

// ---------- XMLString ---------- //

XML_H_API XMLString *xml_string_new() {
  XMLString *str = (XMLString *)malloc(sizeof(XMLString));
  str->len = 0;
  str->size = 64;
  str->str = (char *)malloc(str->size);
  str->str[0] = '\0';
  return str;
}

XML_H_API void xml_string_append(XMLString *str, const char *append) {
  if (!str || !append) return;
  size_t append_len = strlen(append);
  if (str->len + append_len + 1 > str->size) {
    str->size = (str->len + append_len + 1) * 2;
    str->str = (char *)realloc(str->str, str->size);
  }
  strcpy(str->str + str->len, append);
  str->len += append_len;
}

XML_H_API void xml_string_free(XMLString *str) {
  if (!str) return;
  XML_FREE(str->str);
  XML_FREE(str);
}

// ---------- XMLList ---------- //

// Create new dynamic array
XML_H_API XMLList *xml_list_new() {
  XMLList *list = (XMLList *)malloc(sizeof(XMLList));
  list->len = 0;
  list->size = 32;
  list->data = malloc(sizeof(void *) * list->size);
  return list;
}

// Add element to the end of the array. Grow if needed.
XML_H_API void xml_list_add(XMLList *list, void *data) {
  if (!list || !data) return;
  if (list->len >= list->size) {
    list->size *= 2;
    list->data = realloc(list->data, list->size * sizeof(void *));
  }
  list->data[list->len++] = data;
}

// ---------- XMLNode ---------- //

XML_H_API XMLNode *xml_node_new(XMLNode *parent, const char *tag, const char *inner_text) {
  XMLNode *node = malloc(sizeof(XMLNode));
  node->parent = parent;
  node->tag = tag ? strdup(tag) : NULL;
  node->text = inner_text ? strdup(inner_text) : NULL;
  node->children = xml_list_new();
  node->attrs = xml_list_new();
  if (parent) xml_list_add(parent->children, node);
  return node;
}

XML_H_API void xml_node_add_attr(XMLNode *node, const char *key, const char *value) {
  XMLAttr *attr = malloc(sizeof(XMLAttr));
  attr->key = strdup(key);
  attr->value = strdup(value);
  xml_list_add(node->attrs, attr);
}

XML_H_API XMLNode *xml_node_child_at(XMLNode *node, size_t index) {
  if (index > node->children->len - 1) return NULL;
  return (XMLNode *)node->children->data[index];
}

XML_H_API XMLNode *xml_node_find_tag(XMLNode *node, const char *tag, bool exact) {
  if (!node || !tag) return NULL; // Invalid input
  // Check if the current node matches the tag
  if (node->tag && ((exact && strcmp(node->tag, tag) == 0) || (!exact && strstr(node->tag, tag) != NULL))) return node;
  // Recursively search through the children of the node
  for (size_t i = 0; i < node->children->len; i++) {
    XMLNode *result = xml_node_find_tag(node->children->data[i], tag, exact);
    if (result) return result; // Return the first match found
  }
  // No match found in this subtree
  return NULL;
}

XML_H_API XMLNode *xml_node_find_by_path(XMLNode *root, const char *path, bool exact) {
  if (!root || !path) return NULL;
  char *tokenized_path = strdup(path);
  if (!tokenized_path) return NULL;
  char *segment = strtok(tokenized_path, "/");
  XMLNode *current = root;
  while (segment && current) {
    bool found = false;
    for (size_t i = 0; i < current->children->len; i++) {
      XMLNode *child = (XMLNode *)current->children->data[i];
      if ((exact && strcmp(child->tag, segment) == 0) || (!exact && strstr(child->tag, segment) != NULL)) {
        current = child;
        found = true;
        break;
      }
    }
    if (!found) {
      XML_FREE(tokenized_path);
      return NULL;
    }
    segment = strtok(NULL, "/");
  }
  XML_FREE(tokenized_path);
  return current;
}

XML_H_API const char *xml_node_attr(XMLNode *node, const char *attr_key) {
  if (!node || !attr_key) return NULL;
  for (size_t i = 0; i < node->attrs->len; i++) {
    XMLAttr *attr = (XMLAttr *)node->attrs->data[i];
    if (!strcmp(attr->key, attr_key)) return attr->value;
  }
  return NULL;
}

// Trim leading and trailing whitespace from a string (in place).
static void trim_text(char *text) {
  char *start = text;
  while (*start && isspace((unsigned char)*start)) start++;
  char *dest = text;
  while (*start) *dest++ = *start++;
  *dest = '\0';
  dest = text + strlen(text) - 1;
  while (dest >= text && isspace((unsigned char)*dest)) {
    *dest = '\0';
    dest--;
  }
}

// Skip processing instructions and comments
// Returns true if skipped.
static bool skip_tags(const char *xml, size_t *idx) {
  // Processing instruction
  if (xml[*idx] == '?') {
    while (!(xml[*idx] == '>' && xml[*idx - 1] == '?')) (*idx)++;
    (*idx)++;
    return true;
  }
  // Comment
  else if (xml[*idx] == '!' && xml[*idx + 1] == '-' && xml[*idx + 2] == '-') {
    while (!(xml[*idx] == '>' && xml[*idx - 1] == '-' && xml[*idx - 2] == '-')) (*idx)++;
    (*idx)++;
    return true;
  }
  return false;
}

// Parse end tag </tag>.
static void parse_end_tag(const char *xml, size_t *idx, XMLNode **curr_node) {
  (*idx)++; // Skip '/'
  SKIP_WHITESPACE(xml, idx);
  while (xml[*idx] != '>') (*idx)++;
  (*idx)++; // Skip '>'
  *curr_node = (*curr_node)->parent;
}

// Parse tag name <name ... >
static void parse_tag_name(const char *xml, size_t *idx, XMLNode **curr_node) {
  size_t tag_start = *idx;
  while (!(isspace(xml[*idx]) || xml[*idx] == '>' || xml[*idx] == '/')) (*idx)++;
  (*curr_node)->tag = strndup(xml + tag_start, *idx - tag_start);
}

// Parse tag attributes <tag attr="value" ... >
static void parse_tag_attributes(const char *xml, size_t *idx, XMLNode **curr_node) {
  SKIP_WHITESPACE(xml, idx);
  while (!(xml[*idx] == '>' || xml[*idx] == '/')) {
    size_t attr_start = *idx;
    while (xml[*idx] != '\0' && xml[*idx] != '=' && !isspace(xml[*idx])) (*idx)++;
    if (*idx == attr_start) {
      while (xml[*idx] != '\0' && xml[*idx] != '>' && xml[*idx] != '/') { (*idx)++; }
      break;
    }
    size_t attr_len = *idx - attr_start;
    char attr_key[attr_len + 1];
    strncpy(attr_key, xml + attr_start, attr_len);
    attr_key[attr_len] = '\0';
    SKIP_WHITESPACE(xml, idx);
    if (xml[*idx] != '=') break;
    (*idx)++;
    SKIP_WHITESPACE(xml, idx);
    char quote = xml[*idx];
    if (quote != '"' && quote != '\'') break;
    (*idx)++; // Skip opening quote
    size_t value_start = *idx;
    while (xml[*idx] != '\0' && !(xml[*idx] == quote && xml[*idx - 1] != '\\')) { (*idx)++; }
    if (xml[*idx] == '\0') break;
    size_t value_len = *idx - value_start;
    char attr_value[value_len + 1];
    strncpy(attr_value, xml + value_start, value_len);
    attr_value[value_len] = '\0';
    (*idx)++; // Skip closing quote
    xml_node_add_attr(*curr_node, attr_key, attr_value);
    SKIP_WHITESPACE(xml, idx);
  }
}

static void parse_tag_inner_text(const char *xml, size_t *idx, XMLNode **curr_node) {
  size_t text_start = *idx;
  while (xml[*idx] != '<') (*idx)++;
  if (*idx > text_start) {
    (*curr_node)->text = strndup(xml + text_start, *idx - text_start);
    trim_text((*curr_node)->text);
  }
}

// Parse start tag.
// Returns false if the tag is self-closing like: <tag />
// Call continue if returns false.
static bool parse_tag(const char *xml, size_t *idx, XMLNode **curr_node) {
  SKIP_WHITESPACE(xml, idx);
  // End tag </tag>
  if (xml[*idx] == '/') {
    parse_end_tag(xml, idx, curr_node);
    return false;
  }
  // Create new node with current curr_node as parent
  *curr_node = xml_node_new(*curr_node, NULL, NULL);
  // Start tag <tag...>
  parse_tag_name(xml, idx, curr_node);
  // Parse attributes
  if (isspace(xml[*idx])) parse_tag_attributes(xml, idx, curr_node);
  // Self-closing tag <tag ... />
  if (xml[*idx] == '/') {
    (*idx)++; // Skip '/'
    while (xml[*idx] != '>') (*idx)++;
    (*idx)++; // Consume '>'
    *curr_node = (*curr_node)->parent;
    return false;
  }
  // Start tag <tag ... >
  else if (xml[*idx] == '>') {
    (*idx)++; // Consume '>'
    SKIP_WHITESPACE(xml, idx);
    parse_tag_inner_text(xml, idx, curr_node);
    // If the next character is '<', parse the next tag
    if (xml[*idx] == '<') {
      (*idx)++; // Consume '<'
      return parse_tag(xml, idx, curr_node);
    }
    return true;
  }
  SKIP_WHITESPACE(xml, idx);
  return true;
}

XML_H_API XMLNode *xml_parse_string(const char *xml) {
  XMLNode *root = xml_node_new(NULL, NULL, NULL);
  XMLNode *curr_node = root;
  size_t idx = 0;
  while (xml[idx] != '\0') {
    SKIP_WHITESPACE(xml, &idx);
    // Parse tag
    if (xml[idx] == '<') {
      idx++;
      SKIP_WHITESPACE(xml, &idx);
      if (skip_tags(xml, &idx)) continue;
      if (!parse_tag(xml, &idx, &curr_node)) continue;
    }
    idx++;
  }
  return root;
}

XML_H_API XMLNode *xml_parse_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) return NULL;
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *buffer = (char *)malloc(file_size + 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }
  size_t bytes_read = fread(buffer, 1, file_size, file);
  if (bytes_read != file_size) {
    XML_FREE(buffer);
    fclose(file);
    return NULL;
  }
  buffer[file_size] = '\0';
  fclose(file);
  XMLNode *node = xml_parse_string(buffer);
  XML_FREE(buffer);
  return node;
}

XML_H_API void xml_node_serialize(XMLNode *node, XMLString *str) {
  if (!node || !str) return;
  // Opening tag
  if (node->tag) {
    if (!node->text && node->children->len == 0) xml_string_append(str, "</");
    else xml_string_append(str, "<");
    xml_string_append(str, node->tag);
  }
  // Attributes
  if (node->attrs->len > 0) xml_string_append(str, " ");
  for (size_t i = 0; i < node->attrs->len; ++i) {
    XMLAttr *attr = node->attrs->data[i];
    xml_string_append(str, attr->key);
    xml_string_append(str, "=\"");
    xml_string_append(str, attr->value);
    xml_string_append(str, "\"");
    if (i < node->attrs->len - 1) xml_string_append(str, " ");
  }
  if (node->tag) xml_string_append(str, ">");
  // Return if tag is self-closing
  if (node->children->len == 0 && !node->text) return;
  // Text
  if (node->text) xml_string_append(str, node->text);
  // Children
  for (size_t i = 0; i < node->children->len; ++i) {
    XMLNode *child = node->children->data[i];
    xml_node_serialize(child, str);
  }
  // Closing tag
  if (node->tag) {
    xml_string_append(str, "</");
    xml_string_append(str, node->tag);
    xml_string_append(str, ">");
  }
}

XML_H_API void xml_node_free(XMLNode *node) {
  if (!node) return;
  // Free the text
  XML_FREE(node->text);
  // Free the attributes
  for (size_t i = 0; i < node->attrs->len; i++) {
    XMLAttr *attr = (XMLAttr *)node->attrs->data[i];
    XML_FREE(attr->key);
    XML_FREE(attr->value);
    XML_FREE(attr);
  }
  XML_FREE(node->attrs->data);
  XML_FREE(node->attrs);
  // Recursively free the children
  for (size_t i = 0; i < node->children->len; i++) xml_node_free((XMLNode *)node->children->data[i]);
  XML_FREE(node->children->data);
  XML_FREE(node->children);
  // Free the tag
  XML_FREE(node->tag);
  // Free the node itself
  XML_FREE(node);
}

#endif // XML_H_IMPLEMENTATION
