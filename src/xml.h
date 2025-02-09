// MIT License

// Copyright (c) 2024 Vlad Krupinskii <mrvladus@yandex.ru>

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// xml.h - Header-only C/C++ library to parse XML.

#ifndef XML_H
#define XML_H

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// The main object to interact with parsed XML nodes. Represents single XML tag.
typedef struct XMLNode {
  // Tag string.
  char *tag;
  // Inner text of the tag. NULL if tag has no inner text.
  char *text;
  // List of tag attributes. Check "node->attrs->len" if it has items.
  struct XMLList *attrs;
  // Node's parent node. NULL for the root node.
  struct XMLNode *parent;
  // List of tag's sub-tags. Check "node->children->len" if it has items.
  struct XMLList *children;
} XMLNode;

// Tags attribute containing key and value.
// Like: <tag foo_attr="foo_value" bar_attr="bar_value" />
typedef struct XMLAttr {
  char *key;
  char *value;
} XMLAttr;

// Simple dynamic list that only can grow in size.
// Used in XMLNode for list of children and list of tag attributes.
typedef struct XMLList {
  // Capacity of the list in bytes.
  size_t size;
  // Length of the list.
  size_t len;
  // List of pointers to list items.
  void **data;
} XMLList;

// Parse XML string and return root XMLNode.
// Returns NULL for error.
static inline XMLNode *xml_parse_string(const char *xml);

// Parse XML file for given path and return root XMLNode.
// Returns NULL for error.
static inline XMLNode *xml_parse_file(const char *path);

// Get child of the node at index.
// Returns NULL if not found.
static inline XMLNode *xml_node_child_at(XMLNode *node, size_t idx);

// Find xml tag by path like "div/p/href"
static inline XMLNode *xml_node_find_by_path(XMLNode *root, const char *path, bool exact);

// Get first matching tag in the tree.
// If exact is "false" - will return first tag which name contains "tag"
static inline XMLNode *xml_node_find_tag(XMLNode *node, const char *tag, bool exact);

// Get value of the tag attribute.
// Returns NULL if not found.
static inline const char *xml_node_attr(XMLNode *node, const char *attr_key);

// Cleanup node and all it's children.
static inline void xml_node_free(XMLNode *node);

// Macro for looping node children
#define for_child(node, idx) for (size_t idx = 0; idx < node->children->len; idx++)

// --------------------------------------------------------------------------------------------- //
//                                   FUNCTIONS IMPLEMENTATIONS                                   //
// --------------------------------------------------------------------------------------------- //

// Print debug message. Only if XML_H_DEBUG is defined.
#ifdef XML_H_DEBUG
#define LOG_DEBUG(format, ...) fprintf(stderr, "[xml.h] " format "\n", ##__VA_ARGS__)
#else
#define LOG_DEBUG(format, ...)
#endif

// ------ LIST ------ //

// Create new dynamic array
static inline XMLList *xml_list_new() {
  XMLList *list = (XMLList *)malloc(sizeof(XMLList));
  list->len = 0;
  list->size = 1;
  list->data = (void **)malloc(sizeof(void *) * list->size);
  return list;
}

// Add element to the end of the array. Grow if needed.
static inline void xml_list_add(XMLList *list, void *data) {
  if (list->len >= list->size)
    list->data = (void **)realloc(list->data, ++list->size * sizeof(void *));
  list->data[list->len++] = data;
}

// ------ NODE ------ //

// Create new XMLNode. Free with xml_node_free().
static XMLNode *xml_node_new(XMLNode *parent) {
  XMLNode *node = (XMLNode *)malloc(sizeof(XMLNode));
  node->parent = parent;
  node->tag = NULL;
  node->text = NULL;
  node->children = xml_list_new();
  node->attrs = xml_list_new();
  if (parent)
    xml_list_add(parent->children, node);
  return node;
}

// Add XMLAttr to the node's list of attributes
static void xml_node_add_attr(XMLNode *node, char *key, char *value) {
  XMLAttr *attr = (XMLAttr *)malloc(sizeof(XMLAttr));
  attr->key = key;
  attr->value = value;
  xml_list_add(node->attrs, attr);
}

static inline XMLNode *xml_node_child_at(XMLNode *node, size_t index) {
  if (index > node->children->len - 1)
    return NULL;
  return (XMLNode *)node->children->data[index];
}

static inline XMLNode *xml_node_find_tag(XMLNode *node, const char *tag, bool exact) {
  if (!node || !tag)
    return NULL; // Invalid input
  // Check if the current node matches the tag
  if (node->tag &&
      ((exact && strcmp(node->tag, tag) == 0) || (!exact && strstr(node->tag, tag) != NULL)))
    return node;
  // Recursively search through the children of the node
  for (size_t i = 0; i < node->children->len; i++) {
    XMLNode *child = (XMLNode *)node->children->data[i];
    XMLNode *result = xml_node_find_tag(child, tag, exact);
    if (result)
      return result; // Return the first match found
  }
  // No match found in this subtree
  return NULL;
}

static inline XMLNode *xml_node_find_by_path(XMLNode *root, const char *path, bool exact) {
  if (!root || !path)
    return NULL;                       // Invalid input
  char *tokenized_path = strdup(path); // Copy to avoid modifying the input
  if (!tokenized_path)
    return NULL; // Memory allocation failure
  char *segment = strtok(tokenized_path, "/");
  XMLNode *current = root;
  while (segment && current) {
    bool found = false;
    for (size_t i = 0; i < current->children->len; i++) {
      XMLNode *child = (XMLNode *)current->children->data[i];
      if ((exact && strcmp(child->tag, segment) == 0) ||
          (!exact && strstr(child->tag, segment) != NULL)) {
        current = child;
        found = true;
        break;
      }
    }
    if (!found) {
      free(tokenized_path);
      return NULL; // Path not found
    }
    segment = strtok(NULL, "/");
  }
  free(tokenized_path);
  return current;
}

static inline const char *xml_node_attr(XMLNode *node, const char *attr_key) {
  if (!node || !attr_key)
    return NULL;
  for (size_t i = 0; i < node->attrs->len; i++) {
    XMLAttr *attr = (XMLAttr *)node->attrs->data[i];
    if (!strcmp(attr->key, attr_key))
      return attr->value;
  }
  return NULL;
}

static inline void xml_node_free(XMLNode *node) {
  if (node == NULL)
    return;
  // Free text
  if (node->text)
    free(node->text);
  // Free the attributes
  for (size_t i = 0; i < node->attrs->len; i++) {
    XMLAttr *attr = (XMLAttr *)node->attrs->data[i];
    free(attr->key);
    free(attr->value);
    free(attr);
  }
  free(node->attrs->data);
  free(node->attrs);
  // Recursively free the children
  for (size_t i = 0; i < node->children->len; i++)
    xml_node_free((XMLNode *)node->children->data[i]);
  free(node->children->data);
  free(node->children);
  // Free the tag
  if (node->tag)
    free(node->tag);
  // Free the node itself
  free(node);
}

// ------ PARSING FUNCTIONS ------ //

static void strip_new_lines(char *str) {
  char *start = str;
  while (isspace((unsigned char)*start))
    start++;
  char *end = str + strlen(str) - 1;
  while (end > start && isspace((unsigned char)*end))
    end--;
  *(end + 1) = '\0';
  if (start != str)
    memmove(str, start, end - start + 2);
}

static inline XMLNode *xml_parse_string(const char *xml) {
  LOG_DEBUG("Parse string");

  XMLNode *root = xml_node_new(NULL);
  XMLNode *curr_node = root;
  size_t idx = 0;

  while (xml[idx] != '\0') {
    if (xml[idx] == '<') {

      // Processing instruction
      if (xml[idx + 1] == '?') {
        LOG_DEBUG("Parse processing instruction");
        int count = 0; // Count for inner '<' and '>' brackets
        while (true) {
          if (xml[idx] == '<')
            count++;
          else if (xml[idx] == '>') {
            count--;
            if (count <= 0) {
              idx++;
              break;
            }
          }
          idx++;
        }
        idx++; // Move past '>' character
        continue;
      }

      // DOCTYPE
      if (xml[idx + 1] == '!') {
        // Comment
        if (xml[idx + 2] == '-' && xml[idx + 3] == '-') {
          LOG_DEBUG("Parse comment");
          while (xml[idx] != '>')
            idx++;
          idx++; // Move past '>' character
          continue;
        }
      }

      // End tag
      if (xml[idx + 1] == '/') {
        LOG_DEBUG("Parse end tag: %s", curr_node->tag);
        while (xml[idx] != '>')
          idx++;
        idx++; // Move past '>' character
        curr_node = curr_node->parent;
        continue;
      }

      curr_node = xml_node_new(curr_node);

      // Start tag
      LOG_DEBUG("Start parsing tag");
      XMLNode *new_current_node = curr_node;
      idx++; // Move past '<' character
      size_t tag_start = idx;
      while (true) {
        // Self-closing tag without attributes
        if (xml[idx] == '/') {
          LOG_DEBUG("Tag is self-closing without attributes");
          while (xml[idx] != '>')
            idx++;
          new_current_node = curr_node->parent;
          break;
        }

        // Space after tag name
        if (isspace(xml[idx])) {
          // Set tag name
          size_t tag_len = idx - tag_start;
          curr_node->tag = (char *)malloc(sizeof(char) * tag_len + 1);
          strncpy(curr_node->tag, xml + tag_start, tag_len);
          curr_node->tag[tag_len] = '\0';
          LOG_DEBUG("Parsed start tag: %s", curr_node->tag);

          // Skip white space
          while (isspace(xml[idx]))
            idx++;

          // Self-closing tag without attributes
          if (xml[idx] == '/') {
            LOG_DEBUG("Tag is self-closing without attributes");
            while (xml[idx] != '>')
              idx++;
            new_current_node = curr_node->parent;
            break;
          }

          // Tag attributes
          else {
            while (xml[idx] != '>') {
              // Skip white spaces
              if (isspace(xml[idx])) {
                idx++;
                continue;
              }
              // Self-closing tag with attributes
              if (xml[idx] == '/') {
                LOG_DEBUG("Tag is self-closing");
                new_current_node = curr_node->parent;
                idx++;
                continue;
              }
              // Get attribute key
              size_t key_start = idx;
              while (xml[idx] != '=')
                idx++;
              size_t key_len = idx - key_start;
              char *attr_key = (char *)malloc(sizeof(char) * key_len + 1);
              strncpy(attr_key, xml + key_start, key_len);
              attr_key[key_len] = '\0';
              idx += 2; // Skip '=' and '"'
              // Get attribute value
              size_t value_start = idx;
              while (xml[idx] != '"')
                idx++;
              size_t value_len = idx - value_start;
              idx++; // Skip '"'
              char *attr_value = (char *)malloc(sizeof(char) * value_len + 1);
              strncpy(attr_value, xml + value_start, value_len);
              attr_value[value_len] = '\0';
              // Add attribute to node
              xml_node_add_attr(curr_node, attr_key, attr_value);
              LOG_DEBUG("Parse attribute: %s='%s'", attr_key, attr_value);
            }
            break;
          }
        } else if (xml[idx] == '>') {
          // Set tag name
          size_t tag_len = idx - tag_start;
          curr_node->tag = (char *)malloc(sizeof(char) * tag_len + 1);
          strncpy(curr_node->tag, xml + tag_start, tag_len);
          curr_node->tag[tag_len] = '\0';
          LOG_DEBUG("Parsed start tag with no attributes: %s", curr_node->tag);
          break;
        }
        idx++;
      }
      idx++; // Move past '>' character
      curr_node = new_current_node;
      continue;
    }

    // Inner text
    if (!isspace(xml[idx])) {
      LOG_DEBUG("Start parsing text");
      size_t text_start = idx;
      while (true) {
        if (xml[idx] == '<' && xml[idx + 1] == '/')
          break;
        idx++;
      }
      size_t text_len = idx - text_start;
      curr_node->text = (char *)malloc(sizeof(char) * text_len + 1);
      strncpy(curr_node->text, xml + text_start, text_len);
      curr_node->text[text_len] = '\0';
      strip_new_lines(curr_node->text);
      LOG_DEBUG("End parsing text: %s", curr_node->text);
    } else {
      idx++;
    }
  }
  LOG_DEBUG("Done parsing string");
  return root;
}

static inline XMLNode *xml_parse_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *buffer = (char *)malloc(file_size + 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }
  size_t bytes_read = fread(buffer, 1, file_size, file);
  if (bytes_read != file_size) {
    free(buffer);
    fclose(file);
    return NULL;
  }
  buffer[file_size] = '\0';
  fclose(file);
  XMLNode *node = xml_parse_string(buffer);
  free(buffer);
  return node;
}

#ifdef __cplusplus
}
#endif

#endif // XML_H
