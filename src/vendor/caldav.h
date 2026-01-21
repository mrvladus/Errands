// Copyright (c) 2026 Vlad Krupinskii <mrvladus@yandex.ru>
// SPDX-License-Identifier: Zlib

#ifndef CALDAV_H
#define CALDAV_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdbool.h>
#include <stddef.h>

// ---------- REDEFINE ALLOC FUNCTIONS ---------- //

// Users can redefine this macro to use custom memory allocation functions.
//
// Memory allocation function override for calloc
// Defaults to standard `calloc()`
#ifndef CALDAV_CALLOC_FUNC
#define CALDAV_CALLOC_FUNC calloc
#endif // CALDAV_CALLOC_FUNC

// Memory reallocation function override for realloc
#ifndef CALDAV_REALLOC_FUNC
#define CALDAV_REALLOC_FUNC realloc
#endif // CALDAV_REALLOC_FUNC

// String duplication function override for strdup
#ifndef CALDAV_STRDUP_FUNC
#define CALDAV_STRDUP_FUNC strdup
#endif // CALDAV_STRDUP_FUNC

// String duplication with length function override for strndup
#ifndef CALDAV_STRNDUP_FUNC
#define CALDAV_STRNDUP_FUNC strndup
#endif // CALDAV_STRNDUP_FUNC

// Memory deallocation function override for free
#ifndef CALDAV_FREE_FUNC
#define CALDAV_FREE_FUNC free
#endif // CALDAV_FREE_FUNC

// ---------- TYPEDEFS ---------- //

// Bitmask flags for supported calendar component types
// These flags indicate which iCalendar components a calendar supports
// Multiple components can be combined using bitwise OR
typedef enum {
  CALDAV_COMPONENT_SET_VTODO = 1 << 0,     // Supports VTODO (todos)
  CALDAV_COMPONENT_SET_VEVENT = 1 << 1,    // Supports VEVENT (events)
  CALDAV_COMPONENT_SET_VJOURNAL = 1 << 2,  // Supports VJOURNAL (journals)
  CALDAV_COMPONENT_SET_VFREEBUSY = 1 << 3, // Supports VFREEBUSY (free/busy)
} CalDAVComponentSet;

// Forward declarations
typedef struct CalDAVCalendar CalDAVCalendar;
typedef struct CalDAVEvent CalDAVEvent;

// Dynamic array of calendar pointers
// This structure manages a collection of CalDAVCalendar objects
// The array automatically grows as needed
typedef struct {
  size_t count;           // Number of calendars currently in the array
  size_t capacity;        // Current capacity of the array
  CalDAVCalendar **items; // Array of calendar pointers
} CalDAVCalendars;

// Dynamic array of event pointers
// This structure manages a collection of CalDAVEvent objects
// The array automatically grows as needed
typedef struct {
  size_t count;        // Number of events currently in the array
  size_t capacity;     // Current capacity of the array
  CalDAVEvent **items; // Array of event pointers
} CalDAVEvents;

// Main client structure for CalDAV operations
// Represents a connection to a CalDAV server and manages calendars
// All of the fields are "Read-only" and should not be modified directly
typedef struct CalDAVClient {
  // Username for authentication
  char *username;
  // Password for authentication
  char *password;
  // Base URL of the CalDAV server
  char *base_url;
  // Principal URL discovered from server
  char *principal_url;
  // Calendar Home Set URL discovered from server
  char *calendar_home_set_url;
  // Dynamic array of calendars managed by this client
  CalDAVCalendars *calendars;
} CalDAVClient;

// Represents a single calendar on the CalDAV server
// Contains calendar metadata and events. This structure is managed by CalDAVClient
// All of the fields are "Read-only" and should not be modified directly
struct CalDAVCalendar {
  // Parent client that manages this calendar
  CalDAVClient *client;
  // URL of the calendar resource on the server
  char *href;
  // UID of the calendar
  char *uid;
  // Human-readable name of the calendar
  char *display_name;
  // Optional description of the calendar
  char *description;
  // Optional color for calendar display (hex format)
  char *color;
  // Collection tag for synchronization (changes when calendar content changes)
  char *ctag;
  // Bitmask of supported component types
  CalDAVComponentSet component_set;
  // Set to `true` if the calendar has been created or modified on server
  // Reset to `false` when calling `caldav_client_pull_calendars()`
  bool events_changed;
  // Set to `true` if the calendar has been deleted on server
  // Calendar is removed from the calendars list of the client on the next call of `caldav_client_pull_calendars()`
  bool deleted;
  // Dynamic array of events in this calendar
  CalDAVEvents *events;
};

// Represents a single event in a calendar
// Contains event metadata and iCalendar data
// All of the fields are "Read-only" and should not be modified directly
struct CalDAVEvent {
  // Reference to the calendar this event belongs to
  CalDAVCalendar *calendar;
  // Entity tag for synchronization (changes when event content changes)
  char *etag;
  // URL of the event resource on the server
  char *href;
  // iCalendar data for this event (RFC 5545 format)
  char *ical;
  // If event is marked as deleted - it will be removed from the calendar events list
  // and freed on the next `caldav_calendar_pull_events` call
  bool deleted;
};

// ---------- API ---------- //

// Create a new CalDAV client instance
// note: The client must be freed with `caldav_client_free()`
// `base_url`: URL of the CalDAV server (e.g., "https://caldav.example.com" or "example.com")
// `username`: Authentication username
// `password`: Authentication password
// return: New `CalDAVClient` instance, or `NULL` on failure
CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password);

// Free a CalDAV client and all associated resources
// note: This also frees all calendars and events managed by this client
// `c`: Client instance to free
void caldav_client_free(CalDAVClient *c);

// Synchronize calendars with the server
// Downloads calendar list and updates local state
// note: This resets the `properties_changed` and `events_changed` flags on calendars and removes calendars marked as
// `c`: Client instance
// `deleted`
void caldav_client_pull_calendars(CalDAVClient *c);

// Create a new calendar on the server
// note: The calendar will be added to the client's calendars list on success
// `c`: Client instance
// `uid`: Unique identifier for the new calendar
// `display_name`: Human-readable name for the calendar
// `description`: Optional description of the calendar (can be `NULL`)
// `color`: Optional color in hex format (e.g., "#FF0000FF") (can be `NULL`)
// return: `true` if successful, `false` on failure
bool caldav_client_create_calendar(CalDAVClient *c, const char *uid, const char *display_name, const char *description,
                                   const char *color, CalDAVComponentSet set);

// Synchronize events for a specific calendar with the server
// Downloads event list and updates local state
// note: Marked as `deleted` events are removed from the calendar's events list
// `c`: Calendar instance
void caldav_calendar_pull_events(CalDAVCalendar *c);

// Update calendar information on the server
// `c`: Calendar instance to update
// `display_name`: New human-readable name for the calendar (can be `NULL`)
// `description`: New description of the calendar (can be `NULL`)
// `color`: New color in hex format (e.g., "#FF0000FF") (can be `NULL`)
// return: `true` if successful, `false` on failure
bool caldav_calendar_update(CalDAVCalendar *c, const char *display_name, const char *description, const char *color);

// Delete a calendar from the server
// note: The calendar will be marked as deleted and removed on next pull
// `c`: Calendar instance to delete
// return: `true` if successful, `false` on failure
bool caldav_calendar_delete(CalDAVCalendar *c);

// Print calendar information to stdout for debugging
// `c`: Calendar instance to print
void caldav_calendar_print(CalDAVCalendar *c);

// Create a new event in a calendar on the server
// note: The event will be added to the calendar's events list on success
// `c`: Calendar instance
// `uid`: Unique identifier for the new event
// `ical`: iCalendar data for the event (RFC 5545 format)
// return: `true` if successful, `false` on failure
bool caldav_calendar_create_event(CalDAVCalendar *c, const char *uid, const char *ical);

// Update an event on the server
// `e`: Event instance to update
// `ical`: New iCalendar data for the event (RFC 5545 format)
// return: `true` if successful, `false` on failure
bool caldav_event_update(CalDAVEvent *e, const char *ical);

// Delete an event from the server and mark it as deleted if successful
// note: The event will be removed on next pull
// `e`: Event instance to delete
// return: `true` if successful, `false` on failure
bool caldav_event_delete(CalDAVEvent *e);

// Print event information to stdout for debugging
// `e`: Event instance to print
void caldav_event_print(CalDAVEvent *e);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // CALDAV_H

// ------------------------------------------------------------ //
//                         IMPLEMENTATION                       //
// ------------------------------------------------------------ //

#ifdef CALDAV_IMPLEMENTATION

/*

DESCRIPTION:

    da.h - Dynamic array.

LICENSE:

    Copyright (c) 2026 Vlad Krupinskii <mrvladus@yandex.ru>

    This software is provided 'as-is', without any express or implied
    warranty. In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.

*/

#ifndef DA_H
#define DA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdlib.h>

#define DA_CALLOC  CALDAV_CALLOC_FUNC
#define DA_REALLOC CALDAV_REALLOC_FUNC
#define DA_FREE    CALDAV_FREE_FUNC

// Declare new array with the variable "name" and item type "T"
#define da_new(name, T)                                                                                                \
  struct {                                                                                                             \
    size_t count;                                                                                                      \
    size_t capacity;                                                                                                   \
    T *items;                                                                                                          \
  } name = {0}

#define da_count(da)           ((da)->count)
#define da_at(da, idx)         ((da)->items[(idx)])
#define da_first(da)           ((da)->items)
#define da_last(da)            ((da)->items[(da)->count - 1])
#define da_foreach(T, var, da) for (T *var = da_first(da); var < (da)->items + da_count(da); ++var)

// Reserve memory for N elements in the array
#define da_reserve(da, N)                                                                                              \
  do {                                                                                                                 \
    (da)->capacity += (N) > 0 ? (N) : (da)->capacity;                                                                  \
    (da)->items = DA_REALLOC((da)->items, sizeof(*(da)->items) * (da)->capacity);                                      \
  } while (0)

#define da_add(da, item)                                                                                               \
  do {                                                                                                                 \
    if (da_count(da) == (da)->capacity) da_reserve(da, (da)->capacity ? (da)->capacity * 2 : 8);                       \
    da_at(da, da_count(da)++) = (item);                                                                                \
  } while (0)

#define da_remove(da, idx)                                                                                             \
  do {                                                                                                                 \
    if ((idx) < da_count(da)) {                                                                                        \
      for (size_t _i = (idx); _i + 1 < da_count(da); ++_i) da_at(da, _i) = da_at(da, _i + 1);                          \
      --da_count(da);                                                                                                  \
    }                                                                                                                  \
  } while (0)

#define da_remove_full(da, idx, item_free_func)                                                                        \
  do {                                                                                                                 \
    if ((idx) < da_count(da)) {                                                                                        \
      item_free_func(da_at(da, idx));                                                                                  \
      da_remove(da, idx);                                                                                              \
    }                                                                                                                  \
  } while (0)

#define da_remove_fast(da, idx)                                                                                        \
  do {                                                                                                                 \
    if ((idx) < da_count(da) - 1) da_at(da, idx) = da_last(da);                                                        \
    da_count(da)--;                                                                                                    \
  } while (0)

#define da_remove_fast_full(da, idx, item_free_func)                                                                   \
  do {                                                                                                                 \
    item_free_func(da_at(da, idx));                                                                                    \
    da_remove_fast(da, idx);                                                                                           \
  } while (0)

#define da_find(da, item, idx_ptr)                                                                                     \
  do {                                                                                                                 \
    for (size_t _i = 0; _i < da_count(da); _i++) {                                                                     \
      if (da_at(da, _i) == (item)) {                                                                                   \
        *(idx_ptr) = _i;                                                                                               \
        break;                                                                                                         \
      }                                                                                                                \
    }                                                                                                                  \
    *(idx_ptr) = -1;                                                                                                   \
  } while (0)

#define da_free(da)                                                                                                    \
  do {                                                                                                                 \
    DA_FREE((da)->items);                                                                                              \
    (da)->items = NULL;                                                                                                \
    (da)->count = 0;                                                                                                   \
    (da)->capacity = 0;                                                                                                \
  } while (0)

#define da_free_full(da, item_free_func)                                                                               \
  do {                                                                                                                 \
    for (size_t _i = 0; _i < (da)->count; ++_i) item_free_func(da_at(da, _i));                                         \
    da_free(da);                                                                                                       \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif // DA_H

#define XML_H_IMPLEMENTATION
#define XML_H_API static

/*

Modified version of https://github.com/mrvladus/xml.h
Modifications:
    - Removed unused structs and functions
    - Removed examples and changelogs

------------------------------------------------------------------------------

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

*/

#ifndef XML_H
#define XML_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdbool.h>
#include <stddef.h>

// ---------- REDEFINE FUNCTIONS VISIBILITY  ---------- //

#ifndef XML_H_API
#define XML_H_API extern
#endif // XML_H_API

// ---------- REDEFINE ALLOC FUNCTIONS ---------- //

#define XML_CALLOC_FUNC  CALDAV_CALLOC_FUNC
#define XML_REALLOC_FUNC CALDAV_REALLOC_FUNC
#define XML_STRDUP_FUNC  CALDAV_STRDUP_FUNC
#define XML_STRNDUP_FUNC CALDAV_STRNDUP_FUNC
#define XML_FREE_FUNC    CALDAV_FREE_FUNC

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
// Get child of the node at index.
// Returns NULL if not found.
XML_H_API XMLNode *xml_node_child_at(XMLNode *node, size_t idx);
// Get first matching tag in the tree.
// It also can search tags by path in the format: `div/p/href`
// If `exact` is `true` - tag names will be matched exactly.
// If `exact` is `false` - tag names will be matched partially (containing sub-string).
XML_H_API XMLNode *xml_node_find_tag(XMLNode *node, const char *tag, bool exact);
// Get value of the tag attribute.
// Returns NULL if not found.
XML_H_API const char *xml_node_attr(XMLNode *node, const char *attr_key);
// Add attribute with `key` and `value` to the node's list of attributes.
XML_H_API void xml_node_add_attr(XMLNode *node, const char *key, const char *value);
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
    XML_FREE_FUNC(ptr);                                                                                                \
    ptr = NULL;                                                                                                        \
  }

#define SKIP_WHITESPACE(xml_ptr, idx_ptr)                                                                              \
  while (isspace(xml_ptr[*idx_ptr])) (*idx_ptr)++

// ---------- XMLList ---------- //

// Create new dynamic array
XML_H_API XMLList *xml_list_new() {
  XMLList *list = (XMLList *)XML_CALLOC_FUNC(1, sizeof(XMLList));
  list->len = 0;
  list->size = 32;
  list->data = XML_CALLOC_FUNC(1, sizeof(void *) * list->size);
  return list;
}

// Add element to the end of the array. Grow if needed.
XML_H_API void xml_list_add(XMLList *list, void *data) {
  if (!list || !data) return;
  if (list->len >= list->size) {
    list->size *= 2;
    list->data = XML_REALLOC_FUNC(list->data, list->size * sizeof(void *));
  }
  list->data[list->len++] = data;
}

// ---------- XMLNode ---------- //

XML_H_API XMLNode *xml_node_new(XMLNode *parent, const char *tag, const char *inner_text) {
  XMLNode *node = XML_CALLOC_FUNC(1, sizeof(XMLNode));
  node->parent = parent;
  node->tag = tag ? XML_STRDUP_FUNC(tag) : NULL;
  node->text = inner_text ? XML_STRDUP_FUNC(inner_text) : NULL;
  node->children = xml_list_new();
  node->attrs = xml_list_new();
  if (parent) xml_list_add(parent->children, node);
  return node;
}

XML_H_API void xml_node_add_attr(XMLNode *node, const char *key, const char *value) {
  XMLAttr *attr = XML_CALLOC_FUNC(1, sizeof(XMLAttr));
  attr->key = XML_STRDUP_FUNC(key);
  attr->value = XML_STRDUP_FUNC(value);
  xml_list_add(node->attrs, attr);
}

XML_H_API XMLNode *xml_node_child_at(XMLNode *node, size_t index) {
  if (index > node->children->len - 1) return NULL;
  return (XMLNode *)node->children->data[index];
}

XML_H_API XMLNode *xml_node_find_tag(XMLNode *node, const char *tag, bool exact) {
  if (!node || !tag) return NULL;
  // If tag doesn't contain any '/' then it's a single tag search
  if (!strchr(tag, '/')) {
    // Check if the current node matches the tag
    if (node->tag && ((exact && strcmp(node->tag, tag) == 0) || (!exact && strstr(node->tag, tag) != NULL)))
      return node;
    // Recursively search through the children of the node
    for (size_t i = 0; i < node->children->len; i++) {
      XMLNode *result = xml_node_find_tag(node->children->data[i], tag, exact);
      if (result) return result; // Return the first match found
    }
    return NULL;
  }
  // Path tag search
  char *tokenized_path = XML_STRDUP_FUNC(tag);
  if (!tokenized_path) return NULL;
  char *segment = strtok(tokenized_path, "/");
  XMLNode *current = node;
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
  (*curr_node)->tag = XML_STRNDUP_FUNC(xml + tag_start, *idx - tag_start);
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
    (*curr_node)->text = XML_STRNDUP_FUNC(xml + text_start, *idx - text_start);
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

// -------------------------------------------------- //
//                        UTILS                       //
// -------------------------------------------------- //

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- MEMORY ---------- //

#define CALDAV_FREE(ptr)                                                                                               \
  do {                                                                                                                 \
    if (ptr) {                                                                                                         \
      CALDAV_FREE_FUNC(ptr);                                                                                           \
      ptr = NULL;                                                                                                      \
    }                                                                                                                  \
  } while (0)

#define CALDAV_REPLACE_STRING(str, new_str)                                                                            \
  do {                                                                                                                 \
    if (!(str)) {                                                                                                      \
      str = CALDAV_STRDUP_FUNC(new_str);                                                                               \
      break;                                                                                                           \
    }                                                                                                                  \
    if ((new_str) && strcmp((str), (new_str)) != 0) {                                                                  \
      CALDAV_FREE(str);                                                                                                \
      str = CALDAV_STRDUP_FUNC(new_str);                                                                               \
    }                                                                                                                  \
  } while (0)

// ---------- STRING BUILDER ---------- //

typedef struct {
  size_t length;
  char *data;
} StringBuilder;

#define sb_append(sb_ptr, str)                                                                                         \
  do {                                                                                                                 \
    const char *_str = (str);                                                                                          \
    size_t len = strlen(_str);                                                                                         \
    (sb_ptr)->data = (char *)CALDAV_REALLOC_FUNC((sb_ptr)->data, (sb_ptr)->length + len + 1);                          \
    memcpy((sb_ptr)->data + (sb_ptr)->length, _str, len);                                                              \
    (sb_ptr)->length += len;                                                                                           \
    (sb_ptr)->data[(sb_ptr)->length] = '\0';                                                                           \
  } while (0)

#define sb_appendf(sb_ptr, fmt, ...)                                                                                   \
  do {                                                                                                                 \
    size_t len = snprintf(NULL, 0, fmt, __VA_ARGS__);                                                                  \
    (sb_ptr)->data = (char *)CALDAV_REALLOC_FUNC((sb_ptr)->data, (sb_ptr)->length + len + 1);                          \
    snprintf((sb_ptr)->data + (sb_ptr)->length, len + 1, fmt, __VA_ARGS__);                                            \
    (sb_ptr)->length += len;                                                                                           \
    (sb_ptr)->data[(sb_ptr)->length] = '\0';                                                                           \
  } while (0)

#define sb_free_data(sb_ptr)                                                                                           \
  if ((sb_ptr)->data) CALDAV_FREE_FUNC((sb_ptr)->data)

// ---------- LOGGING ---------- //

static inline void caldav__log(const char *fmt, ...) {
#ifdef CALDAV_DEBUG
  fprintf(stderr, "CalDAV: ");
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
#endif
}

// ---------- STRINGS ---------- //

// #define STR_HAS_PREFIX(str, prefix) (strncmp(str, prefix, strlen(prefix)) == 0)

// Function to duplicate a string and format it using printf-style arguments.
// The returned string is dynamically allocated and should be freed by the caller.
static inline char *caldav__strdup_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  va_list ap2;
  va_copy(ap2, ap);
  int needed = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  if (needed < 0) {
    va_end(ap2);
    return NULL;
  }
  char *buf = (char *)malloc(needed + 1);
  if (!buf) {
    va_end(ap2);
    return NULL;
  }
  vsnprintf(buf, needed + 1, fmt, ap2);
  va_end(ap2);

  return buf;
}

static inline const char *caldav__url_no_proto(const char *url) {
  if (!url) return NULL;
  const char *protocol_start = strstr(url, "://");
  if (!protocol_start) return url;
  return protocol_start + 3;
}

static inline char *caldav__url_http(const char *url) {
  if (!url) return NULL;
  const char *no_proto = caldav__url_no_proto(url);
  const char *last_slash = strrchr(url, '/');
  bool has_slash = false;
  if (last_slash && last_slash == url + strlen(url) - 1) has_slash = true;
  return caldav__strdup_printf("http://%s%s", no_proto, has_slash ? "" : "/");
}

static inline char *caldav__url_https(const char *url) {
  if (!url) return NULL;
  const char *no_proto = caldav__url_no_proto(url);
  const char *last_slash = strrchr(url, '/');
  bool has_slash = false;
  if (last_slash && last_slash == url + strlen(url) - 1) has_slash = true;
  return caldav__strdup_printf("https://%s%s", no_proto, has_slash ? "" : "/");
}

static inline char *caldav__extract_base_url(const char *url) {
  if (!url) return NULL;
  const char *protocol_end = strstr(url, "://");
  if (!protocol_end) return NULL;
  const char *host_end = strchr(protocol_end + 3, '/');
  return host_end ? caldav__strdup_printf("%.*s", host_end - url, url) : CALDAV_STRDUP_FUNC(url);
}

static char *caldav__calendar_uid_from_href(const char *href) {
  char *last_slash = strchr(href, '/');
  bool ends_with_slash = false;
  while (last_slash) {
    char *next_slash = strchr(last_slash + 1, '/');
    if (!next_slash) break;
    if (*(next_slash + 1) == '\0') {
      ends_with_slash = true;
      break;
    }
    last_slash = next_slash;
  }
  char *out = caldav__strdup_printf("%s", last_slash + 1);
  if (ends_with_slash) out[strlen(out) - 1] = '\0';

  return out;
}

// ---------- FIND FUNCTIONS ---------- //

static CalDAVCalendar *caldav__find_calendar_by_href(CalDAVCalendars *cals, const char *href) {
  if (!cals || !href) return NULL;
  for (size_t i = 0; i < cals->count; ++i) {
    CalDAVCalendar *cal = da_at(cals, i);
    if (strcmp(cal->href, href) == 0) return cal;
  }
  return NULL;
}

static CalDAVEvent *caldav__find_event_by_href(CalDAVEvents *evs, const char *href) {
  if (!evs || !href) return NULL;
  for (size_t i = 0; i < evs->count; ++i) {
    CalDAVEvent *event = da_at(evs, i);
    if (strcmp(event->href, href) == 0) return event;
  }
  return NULL;
}

// ----------------------------------------------- //
//                     REQUESTS                    //
// ----------------------------------------------- //

#include <curl/curl.h>

typedef struct {
  size_t size;
  char *data;
} CalDAVBuffer;

static size_t caldav__write_cb(void *ptr, size_t size, size_t nmemb, void *data) {
  size_t total = size * nmemb;
  CalDAVBuffer *buf = (CalDAVBuffer *)data;
  char *new_data = (char *)CALDAV_REALLOC_FUNC(buf->data, buf->size + total + 1);
  if (!new_data) return 0;
  buf->data = new_data;
  memcpy(buf->data + buf->size, ptr, total);
  buf->size += total;
  buf->data[buf->size] = '\0';

  return total;
}

// No-op callback to discard response body
static size_t caldav__null_write_cb(void *ptr, size_t size, size_t nmemb, void *data) {
  (void)ptr;
  (void)data;
  return size * nmemb;
}

static char *caldav_request(CalDAVClient *c, const char *url, const char *method, struct curl_slist *headers,
                            const char *body, bool *success) {
  if (!c || !url || !method) return NULL;
  CURL *curl = curl_easy_init();
  if (!curl) return NULL;
  CalDAVBuffer buf = {0};
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, caldav__write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
  if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  if (c->username) curl_easy_setopt(curl, CURLOPT_USERNAME, c->username);
  if (c->password) curl_easy_setopt(curl, CURLOPT_PASSWORD, c->password);
  if (body) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
  CURLcode res = curl_easy_perform(curl);
  caldav__log("Status code: %d", res);
  if (res != CURLE_OK) {
    caldav__log("Request failed: %s", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    if (success) *success = false;
    return NULL;
  }
  curl_easy_cleanup(curl);
  if (success) *success = true;

  return buf.data;
}

static char *caldav__request_autodiscover(const char *url) {
  if (!url) return NULL;
  CURL *curl = curl_easy_init();
  if (!curl) return NULL;
  char *https = caldav__url_https(url);
  char *discover_url_https = caldav__strdup_printf("%s/.well-known/caldav", https);
  char *http = caldav__url_http(url);
  char *discover_url_http = caldav__strdup_printf("%s/.well-known/caldav", http);
  caldav__log("Trying to discover CalDAV URL");
  curl_easy_setopt(curl, CURLOPT_URL, discover_url_https);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, caldav__null_write_cb);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    curl_easy_setopt(curl, CURLOPT_URL, discover_url_http);
    res = curl_easy_perform(curl);
  }
  CALDAV_FREE(https);
  CALDAV_FREE(http);
  CALDAV_FREE(discover_url_https);
  CALDAV_FREE(discover_url_http);
  char *caldav_url = NULL;
  if (res != CURLE_OK) caldav__log("Failed to get CalDAV URL: %s", curl_easy_strerror(res));
  else {
    const char *effective_url = NULL;
    res = curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    if (res == CURLE_OK && effective_url) {
      caldav_url = CALDAV_STRDUP_FUNC(effective_url);
      caldav__log("Discovered CalDAV URL: %s", caldav_url);
    } else caldav__log("Failed to discover CalDAV URL");
  }
  curl_easy_cleanup(curl);

  return caldav_url;
}

static char *caldav__request_principal(CalDAVClient *c, const char *url) {
  if (!c || !url) return NULL;
  caldav__log("Getting principal URL for %s", url);
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 0");
  headers = curl_slist_append(headers, "Prefer: return-minimal");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  const char *body = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                     "<propfind xmlns=\"DAV:\">"
                     "  <prop>"
                     "    <current-user-principal/>"
                     "  </prop>"
                     "</propfind>";
  char *https = caldav__url_https(url);
  char *http = caldav__url_http(url);
  const char *effective_url = NULL;
  char *response = NULL;
  response = caldav_request(c, https, "PROPFIND", headers, body, NULL);
  if (!response) {
    response = caldav_request(c, http, "PROPFIND", headers, body, NULL);
    effective_url = http;
  } else effective_url = https;
  curl_slist_free_all(headers);
  if (!response) {
    CALDAV_FREE(http);
    CALDAV_FREE(https);
    return NULL;
  }
  // Parse response
  XMLNode *root = xml_parse_string(response);
  CALDAV_FREE(response);
  if (!root) return NULL;
  XMLNode *multistatus = xml_node_child_at(root, 0);
  if (!multistatus || multistatus->children->len == 0) {
    CALDAV_FREE(http);
    CALDAV_FREE(https);
    xml_node_free(root);
    return NULL;
  }
  XMLNode *response_node = xml_node_child_at(multistatus, 0);
  if (!response_node) {
    CALDAV_FREE(http);
    CALDAV_FREE(https);
    xml_node_free(root);
    return NULL;
  }
  XMLNode *status = xml_node_find_tag(response_node, "propstat/status", false);
  if (!status || strcmp(status->text, "HTTP/1.1 200 OK") != 0) return NULL;
  XMLNode *href = xml_node_find_tag(response_node, "propstat/prop/current-user-principal/href", false);
  if (!href || !href->text) {
    CALDAV_FREE(http);
    CALDAV_FREE(https);
    xml_node_free(root);
    return NULL;
  }
  char *base_url = caldav__extract_base_url(effective_url);
  assert(base_url);
  char *res = caldav__strdup_printf("%s%s", base_url, href->text);
  CALDAV_FREE(http);
  CALDAV_FREE(https);
  CALDAV_FREE(base_url);
  xml_node_free(root);
  caldav__log("Principal URL: %s", res);

  return res;
}

static char *caldav__request_calendars_home_set(CalDAVClient *c) {
  if (!c || !c->principal_url) return NULL;
  caldav__log("Getting calendars home URL");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 0");
  headers = curl_slist_append(headers, "Prefer: return-minimal");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  const char *body = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                     "<propfind xmlns=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                     "  <prop>"
                     "    <c:calendar-home-set/>"
                     "  </prop>"
                     "</propfind>";
  char *response = caldav_request(c, c->principal_url, "PROPFIND", headers, body, NULL);
  curl_slist_free_all(headers);
  if (!response) return NULL;
  // Parse response
  XMLNode *root = xml_parse_string(response);
  CALDAV_FREE(response);
  if (!root) return NULL;
  XMLNode *multistatus = xml_node_child_at(root, 0);
  if (!multistatus || multistatus->children->len == 0) {
    xml_node_free(root);
    return NULL;
  }
  XMLNode *response_node = xml_node_child_at(multistatus, 0);
  if (!response_node) {
    xml_node_free(root);
    return NULL;
  }
  XMLNode *status = xml_node_find_tag(response_node, "propstat/status", false);
  if (!status || strcmp(status->text, "HTTP/1.1 200 OK") != 0) return NULL;
  XMLNode *href = xml_node_find_tag(response_node, "propstat/prop/calendar-home-set/href", false);
  if (!href || !href->text) {
    xml_node_free(root);
    return NULL;
  }
  char *res = caldav__strdup_printf("%s%s", c->base_url, href->text);
  xml_node_free(root);
  caldav__log("Home URL: %s", res);

  return res;
}

static char *caldav_request_get_calendars_info(CalDAVClient *c) {
  if (!c) return NULL;
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 1");
  headers = curl_slist_append(headers, "Prefer: return-minimal");
  headers = curl_slist_append(headers, "Content-Type: application/xml");
  const char *body = "<?xml version=\"1.0\"?>"
                     "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\" "
                     "xmlns:ical=\"http://apple.com/ns/ical/\" xmlns:cs=\"http://calendarserver.org/ns/\">"
                     "  <d:prop>"
                     "    <d:displayname/>"
                     "    <d:resourcetype/>"
                     "    <cs:getctag/>"
                     "    <c:calendar-description/>"
                     "    <c:supported-calendar-component-set/>"
                     "    <ical:calendar-color/>"
                     "  </d:prop>"
                     "</d:propfind>";
  char *response = caldav_request(c, c->calendar_home_set_url, "PROPFIND", headers, body, NULL);
  curl_slist_free_all(headers);
  return response;
}

static bool caldav_request_create_calendar(CalDAVClient *c, const char *uid, const char *display_name,
                                           const char *description, const char *color, CalDAVComponentSet set) {
  if (!c || !uid) return false;
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  const char *body_template =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<create xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\" xmlns:ical=\"http://apple.com/ns/ical/\" "
      "xmlns:oc=\"http://owncloud.org/ns\">"
      "  <d:set>"
      "    <d:prop>"
      "      <d:resourcetype>"
      "        <c:calendar/>"
      "        <d:collection/>"
      "      </d:resourcetype>"
      "      <c:supported-calendar-component-set>%s</c:supported-calendar-component-set>"
      "      <d:displayname>%s</d:displayname>"
      "      <c:calendar-description>%s</c:calendar-description>"
      "      <ical:calendar-color>%s</ical:calendar-color>"
      "      <oc:calendar-enabled>1</oc:calendar-enabled>"
      "    </d:prop>"
      "  </d:set>"
      "</create>";
  StringBuilder sb = {0};
  if (set & CALDAV_COMPONENT_SET_VEVENT) sb_append(&sb, "<c:comp name=\"VEVENT\"/>");
  if (set & CALDAV_COMPONENT_SET_VTODO) sb_append(&sb, "<c:comp name=\"VTODO\"/>");
  if (set & CALDAV_COMPONENT_SET_VJOURNAL) sb_append(&sb, "<c:comp name=\"VJOURNAL\"/>");
  if (set & CALDAV_COMPONENT_SET_VFREEBUSY) sb_append(&sb, "<c:comp name=\"VFREEBUSY\"/>");
  char *body =
      caldav__strdup_printf(body_template, sb.length > 0 ? sb.data : "", display_name ? display_name : "Untitled",
                            description ? description : "", color ? color : "#000000ff");
  sb_free_data(&sb);
  char *url = caldav__strdup_printf("%s%s", c->calendar_home_set_url, uid);
  bool success = false;
  caldav_request(c, url, "MKCOL", headers, body, &success);
  CALDAV_FREE(body);
  curl_slist_free_all(headers);
  if (!success) {
    caldav__log("Failed to create calendar: %s", url);
    CALDAV_FREE(url);
    return false;
  };
  caldav__log("Created calendar: %s", url);
  CALDAV_FREE(url);
  return true;
}

static bool caldav_request_calendar_update(CalDAVCalendar *c, const char *display_name, const char *description,
                                           const char *color) {
  if (!c) return false;
  caldav__log("Update calendar: %s", c->href);
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  StringBuilder sb = {0};
  sb_append(&sb, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                 "<d:propertyupdate xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\" "
                 "xmlns:ical=\"http://apple.com/ns/ical/\">"
                 "<d:set><d:prop>");
  if (display_name) sb_appendf(&sb, "<d:displayname>%s</d:displayname>", display_name);
  if (description) sb_appendf(&sb, "<c:calendar-description>%s</c:calendar-description>", description);
  if (color) sb_appendf(&sb, "<ical:calendar-color>%s</ical:calendar-color>", color);
  sb_append(&sb, "</d:prop></d:set></d:propertyupdate>");
  bool res = false;
  caldav_request(c->client, c->href, "PROPPATCH", headers, sb.data, &res);
  sb_free_data(&sb);
  if (res) {
    if (display_name) CALDAV_REPLACE_STRING(c->display_name, display_name);
    if (description) CALDAV_REPLACE_STRING(c->description, description);
    if (color) CALDAV_REPLACE_STRING(c->color, color);
  }
  return res;
}

static char *caldav_request_calendar_get_etags(CalDAVCalendar *c) {
  caldav__log("Request: ETags");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 1");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  const char *body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                     "<c:calendar-query xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                     "  <d:prop>"
                     "    <d:getetag/>"
                     "  </d:prop>"
                     "  <c:filter>"
                     "    <c:comp-filter name=\"VCALENDAR\"/>"
                     "  </c:filter>"
                     "</c:calendar-query>";
  char *response = caldav_request(c->client, c->href, "REPORT", headers, body, NULL);
  curl_slist_free_all(headers);
  return response;
}

static char *caldav_request_calendar_get_props(CalDAVCalendar *c) {
  caldav__log("Request: Calendar properties");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 0");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  const char *body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                     "<c:calendar-query xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\" "
                     "xmlns:ical=\"http://apple.com/ns/ical/\" xmlns:cs=\"http://calendarserver.org/ns/\">"
                     "  <d:prop>"
                     "    <d:displayname/>"
                     "    <cs:getctag/>"
                     "    <c:calendar-description/>"
                     "    <ical:calendar-color/>"
                     "  </d:prop>"
                     "</c:calendar-query>";
  char *response = caldav_request(c->client, c->href, "PROPFIND", headers, body, NULL);
  curl_slist_free_all(headers);
  return response;
}

static char *caldav_request_calendar_multiget(CalDAVCalendar *c, CalDAVEvents *events) {
  caldav__log("Request: calendar-multiget calendar-data");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 1");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  StringBuilder sb = {0};
  sb_append(&sb, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                 "<c:calendar-multiget xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                 "<d:prop><d:getetag/><c:calendar-data/></d:prop>");
  for (size_t i = 0; i < events->count; ++i) sb_appendf(&sb, "<d:href>%s</d:href>", da_at(events, i)->href);
  sb_append(&sb, "</c:calendar-multiget>");
  char *response = caldav_request(c->client, c->href, "REPORT", headers, sb.data, NULL);
  sb_free_data(&sb);
  curl_slist_free_all(headers);
  return response;
}

static char *caldav_request_calendar_multiget_etags(CalDAVCalendar *c, CalDAVEvents *events) {
  caldav__log("Request: calendar-multiget etag");
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Depth: 1");
  headers = curl_slist_append(headers, "Content-Type: application/xml; charset=utf-8");
  StringBuilder sb = {0};
  sb_append(&sb, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                 "<c:calendar-multiget xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                 "<d:prop><d:getetag/></d:prop>");
  for (size_t i = 0; i < events->count; ++i) sb_appendf(&sb, "<d:href>%s</d:href>", da_at(events, i)->href);
  sb_append(&sb, "</c:calendar-multiget>");
  char *response = caldav_request(c->client, c->href, "REPORT", headers, sb.data, NULL);
  sb_free_data(&sb);
  curl_slist_free_all(headers);
  return response;
}

static bool caldav_request_calendar_create_event(CalDAVCalendar *c, const char *uid, const char *ical) {
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: text/calendar; charset=utf-8");
  char *url = caldav__strdup_printf("%s%s.ics", c->href, uid);
  caldav__log("Creating event: %s", url);
  bool res = false;
  caldav_request(c->client, url, "PUT", headers, ical, &res);
  CALDAV_FREE(url);
  curl_slist_free_all(headers);
  return res;
}

// ----------------------------------------------- //
//                      CLIENT                     //
// ----------------------------------------------- //

static CalDAVCalendar *caldav__calendar_new(CalDAVClient *client, CalDAVComponentSet set, const char *href,
                                            const char *display_name, const char *description, const char *color,
                                            const char *ctag);
static void caldav__calendar_free(CalDAVCalendar *c);

CalDAVClient *caldav_client_new(const char *base_url, const char *username, const char *password) {
  if (!base_url && !username && !password) return NULL;
  CalDAVClient *client = CALDAV_CALLOC_FUNC(1, sizeof(CalDAVClient));
  if (!client) return NULL;
  client->username = CALDAV_STRDUP_FUNC(username);
  client->password = CALDAV_STRDUP_FUNC(password);
  client->principal_url = caldav__request_principal(client, base_url);
  if (!client->principal_url) {
    char *discovered_url = caldav__request_autodiscover(base_url);
    if (!discovered_url) {
      caldav_client_free(client);
      return NULL;
    }
    client->principal_url = caldav__request_principal(client, discovered_url);
    if (!client->principal_url) {
      caldav_client_free(client);
      return NULL;
    }
  }
  client->base_url = caldav__extract_base_url(client->principal_url);
  client->calendar_home_set_url = caldav__request_calendars_home_set(client);
  if (!client->calendar_home_set_url) {
    caldav_client_free(client);
    return NULL;
  }
  client->calendars = CALDAV_CALLOC_FUNC(1, sizeof(*(client->calendars)));

  return client;
}

void caldav_client_free(CalDAVClient *c) {
  if (!c) return;
  CALDAV_FREE(c->username);
  CALDAV_FREE(c->password);
  CALDAV_FREE(c->base_url);
  CALDAV_FREE(c->principal_url);
  CALDAV_FREE(c->calendar_home_set_url);
  if (c->calendars) {
    for (size_t i = 0; i < c->calendars->count; ++i) caldav__calendar_free(c->calendars->items[i]);
    CALDAV_FREE(c->calendars);
  }
  CALDAV_FREE(c);
}

void caldav_client_pull_calendars(CalDAVClient *c) {
  if (!c) return;
  // Remove deleted calendars before pulling new ones
  for (size_t i = 0; i < c->calendars->count; ++i) {
    CalDAVCalendar *calendar = da_at(c->calendars, i);
    calendar->events_changed = false;
    if (calendar->deleted) {
      da_remove(c->calendars, i);
      i--;
    }
  }
  // Pull events
  caldav__log("Pulling calendars from %s", c->calendar_home_set_url);
  // Perform a request
  char *response = caldav_request_get_calendars_info(c);
  if (!response) return;
  // Parse response
  XMLNode *root = xml_parse_string(response);
  CALDAV_FREE(response);
  if (!root) return;
  XMLNode *multistatus = xml_node_child_at(root, 0);
  if (!multistatus) {
    xml_node_free(root);
    return;
  }
  CalDAVCalendars new_calendars = {0};
  for (size_t i = 0; i < multistatus->children->len; ++i) {
    XMLNode *res = xml_node_child_at(multistatus, i);
    if (!xml_node_find_tag(res, "propstat/prop/resourcetype/calendar", false)) continue; // Not a calendar
    XMLNode *status = xml_node_find_tag(res, "propstat/status", false);
    if (!status || strcmp(status->text, "HTTP/1.1 200 OK") != 0) continue;
    XMLNode *component_set = xml_node_find_tag(res, "propstat/prop/supported-calendar-component-set", false);
    if (!component_set || component_set->children->len == 0) continue;
    XMLNode *href = xml_node_find_tag(res, "href", false);
    if (!href || !href->text) continue;
    XMLNode *display_name = xml_node_find_tag(res, "propstat/prop/displayname", false);
    XMLNode *calendar_description = xml_node_find_tag(res, "propstat/prop/calendar-description", false);
    XMLNode *calendar_color = xml_node_find_tag(res, "propstat/prop/calendar-color", false);
    XMLNode *ctag = xml_node_find_tag(res, "propstat/prop/getctag", false);
    // Parse component set.
    CalDAVComponentSet set = 0;
    for (size_t j = 0; j < component_set->children->len; ++j) {
      XMLNode *set_child = xml_node_child_at(component_set, j);
      const char *component_set_name = xml_node_attr(set_child, "name");
      if (component_set) {
        if (strcmp(component_set_name, "VEVENT") == 0) set |= CALDAV_COMPONENT_SET_VEVENT;
        else if (strcmp(component_set_name, "VTODO") == 0) set |= CALDAV_COMPONENT_SET_VTODO;
        else if (strcmp(component_set_name, "VJOURNAL") == 0) set |= CALDAV_COMPONENT_SET_VJOURNAL;
        else if (strcmp(component_set_name, "VFREEBUSY") == 0) set |= CALDAV_COMPONENT_SET_VFREEBUSY;
      }
    }
    char *url = caldav__strdup_printf("%s%s", c->base_url, href->text);
    const char *name = display_name && display_name->text ? display_name->text : NULL;
    const char *desc = calendar_description && calendar_description->text ? calendar_description->text : NULL;
    const char *color = calendar_color && calendar_color->text ? calendar_color->text : NULL;
    const char *c_tag = ctag && ctag->text ? ctag->text : NULL;
    // Check for existing calendar
    CalDAVCalendar *existing_calendar = caldav__find_calendar_by_href(c->calendars, url);
    if (existing_calendar) {
      bool props_changed = false;
      if (name) {
        if (!existing_calendar->display_name || strcmp(name, existing_calendar->display_name) != 0) {
          CALDAV_REPLACE_STRING(existing_calendar->display_name, name);
          props_changed = true;
        }
      }
      if (desc) {
        if (!existing_calendar->description || strcmp(desc, existing_calendar->description) != 0) {
          CALDAV_REPLACE_STRING(existing_calendar->description, desc);
          props_changed = true;
        }
      }
      if (color) {
        if (!existing_calendar->color || strcmp(color, existing_calendar->color) != 0) {
          CALDAV_REPLACE_STRING(existing_calendar->color, color);
          props_changed = true;
        }
      }
      if (props_changed) caldav__log("Calendar properties is changed: %s", url);
      if (c_tag && strcmp(ctag->text, existing_calendar->ctag) != 0) {
        caldav__log("Calendar events is changed: %s", url);
        if (ctag) CALDAV_REPLACE_STRING(existing_calendar->ctag, c_tag);
        existing_calendar->events_changed = true;
      }
      da_add(&new_calendars, existing_calendar);
    } else {
      // Create calendar
      CalDAVCalendar *new_cal = caldav__calendar_new(c, set, url, name, desc, color, c_tag);
      new_cal->events_changed = true;
      da_add(c->calendars, new_cal);
      da_add(&new_calendars, new_cal);
    }
    CALDAV_FREE(url);
  }
  // Deleted calendars
  for (size_t i = 0; i < c->calendars->count; ++i) {
    CalDAVCalendar *cal = da_at(c->calendars, i);
    CalDAVCalendar *existing_cal = caldav__find_calendar_by_href(&new_calendars, cal->href);
    if (!existing_cal) {
      caldav__log("Calendar was deleted: %s", cal->href);
      cal->deleted = true;
    }
  }
  da_free(&new_calendars);
  xml_node_free(root);
}

bool caldav_client_create_calendar(CalDAVClient *c, const char *uid, const char *display_name, const char *description,
                                   const char *color, CalDAVComponentSet set) {
  if (!c || !uid) return false;
  return caldav_request_create_calendar(c, uid, display_name, description, color, set);
}

// ----------------------------------------------- //
//                     CALENDAR                    //
// ----------------------------------------------- //

static CalDAVEvent *caldav__event_new(CalDAVCalendar *c, const char *href, const char *ical, const char *etag);
static void caldav__event_free(CalDAVEvent *e);

static CalDAVCalendar *caldav__calendar_new(CalDAVClient *client, CalDAVComponentSet set, const char *href,
                                            const char *display_name, const char *description, const char *color,
                                            const char *ctag) {
  CalDAVCalendar *calendar = CALDAV_CALLOC_FUNC(1, sizeof(CalDAVCalendar));
  calendar->client = client;
  calendar->component_set = set;
  calendar->href = href ? CALDAV_STRDUP_FUNC(href) : NULL;
  calendar->display_name = display_name ? CALDAV_STRDUP_FUNC(display_name) : NULL;
  calendar->description = description ? CALDAV_STRDUP_FUNC(description) : NULL;
  calendar->color = color ? CALDAV_STRDUP_FUNC(color) : NULL;
  calendar->ctag = ctag ? CALDAV_STRDUP_FUNC(ctag) : NULL;
  calendar->events = CALDAV_CALLOC_FUNC(1, sizeof(*(calendar->events)));
  calendar->uid = caldav__calendar_uid_from_href(calendar->href);

  return calendar;
}

static void caldav__calendar_free(CalDAVCalendar *c) {
  if (!c) return;
  CALDAV_FREE(c->href);
  CALDAV_FREE(c->display_name);
  CALDAV_FREE(c->description);
  CALDAV_FREE(c->color);
  CALDAV_FREE(c->ctag);
  if (c->events) {
    for (size_t i = 0; i < c->events->count; ++i) caldav__event_free(c->events->items[i]);
    CALDAV_FREE(c->events);
  }
  CALDAV_FREE(c);
}

void caldav_calendar_print(CalDAVCalendar *c) {
  if (!c) return;
  printf("---------- CALENDAR ----------\n"
         "Href: %s\n"
         "CTag: %s\n"
         "Display Name: %s\n"
         "Description: %s\n"
         "Color: %s\n"
         "Events Changed: %s\n"
         "------------------------------\n",
         c->href, c->ctag, c->display_name, c->description, c->color, c->events_changed ? "true" : "false");
}

bool caldav_calendar_delete(CalDAVCalendar *c) {
  if (!c) return false;
  caldav__log("Deleting calendar %s", c->href);
  bool res = false;
  char *response = caldav_request(c->client, c->href, "DELETE", NULL, NULL, &res);
  CALDAV_FREE(response);
  if (!res) return false;
  c->deleted = true;
  return true;
}

void caldav_calendar_pull_events(CalDAVCalendar *c) {
  // Remove previously deleted events
  for (size_t i = 0; i < c->events->count; ++i) {
    CalDAVEvent *e = da_at(c->events, i);
    if (e->deleted) {
      da_remove(c->events, i);
      i--;
    }
  }
  // Pull changed and new events
  caldav__log("Pulling events %s", c->href);
  char *response = caldav_request_calendar_get_etags(c);
  if (!response) return;
  XMLNode *root = xml_parse_string(response);
  CALDAV_FREE(response);
  if (!root) return;
  XMLNode *multistatus = xml_node_child_at(root, 0);
  if (!multistatus) {
    xml_node_free(root);
    return;
  }
  CalDAVEvents changed = {0};
  for (size_t i = 0; i < multistatus->children->len; ++i) {
    XMLNode *res = xml_node_child_at(multistatus, i);
    if (!res) continue;
    XMLNode *status = xml_node_find_tag(res, "propstat/status", false);
    if (!status || strcmp(status->text, "HTTP/1.1 200 OK") != 0) continue;
    XMLNode *href = xml_node_find_tag(res, "href", false);
    if (!href || !href->text) continue;
    XMLNode *etag = xml_node_find_tag(res, "propstat/prop/getetag", false);
    if (!etag || !etag->text) continue;
    char *url = caldav__strdup_printf("%s%s", c->client->base_url, href->text);
    CalDAVEvent *existing_event = caldav__find_event_by_href(c->events, url);
    if (existing_event) {
      if (strcmp(etag->text, existing_event->etag) != 0) {
        CALDAV_REPLACE_STRING(existing_event->etag, etag->text);
        da_add(&changed, existing_event);
      }
    } else {
      CalDAVEvent *event = caldav__event_new(c, url, NULL, etag->text);
      da_add(&changed, event);
      da_add(c->events, event);
    }
    CALDAV_FREE(url);
  }
  xml_node_free(root);
  // Get changed events ical data
  response = caldav_request_calendar_multiget(c, &changed);
  if (!response) return;
  root = xml_parse_string(response);
  CALDAV_FREE(response);
  if (!root) return;
  multistatus = xml_node_child_at(root, 0);
  if (!multistatus) {
    xml_node_free(root);
    return;
  }
  for (size_t i = 0; i < multistatus->children->len; ++i) {
    XMLNode *res = xml_node_child_at(multistatus, i);
    if (!res) continue;
    XMLNode *status = xml_node_find_tag(res, "propstat/status", false);
    if (!status || strcmp(status->text, "HTTP/1.1 200 OK") != 0) continue;
    XMLNode *href = xml_node_find_tag(res, "href", false);
    if (!href || !href->text) continue;
    XMLNode *ical = xml_node_find_tag(res, "propstat/prop/calendar-data", false);
    if (!ical || !ical->text) continue;
    char *url = caldav__strdup_printf("%s%s", c->client->base_url, href->text);
    CalDAVEvent *event = caldav__find_event_by_href(&changed, url);
    if (event) CALDAV_REPLACE_STRING(event->ical, ical->text);
    CALDAV_FREE(url);
  }
  CALDAV_FREE(changed.items);
  // Find deleted events
  response = caldav_request_calendar_multiget_etags(c, c->events);
  if (!response) return;
  root = xml_parse_string(response);
  CALDAV_FREE(response);
  if (!root) return;
  multistatus = xml_node_child_at(root, 0);
  if (!multistatus) {
    xml_node_free(root);
    return;
  }
  for (size_t i = 0; i < multistatus->children->len; ++i) {
    XMLNode *res = xml_node_child_at(multistatus, i);
    if (!res) continue;
    XMLNode *status = xml_node_find_tag(res, "status", false);
    if (!status || strcmp(status->text, "HTTP/1.1 404 Not Found") != 0) continue;
    XMLNode *href = xml_node_find_tag(res, "href", false);
    if (!href || !href->text) continue;
    char *url = caldav__strdup_printf("%s%s", c->client->base_url, href->text);
    CalDAVEvent *event = caldav__find_event_by_href(&changed, url);
    if (event) event->deleted = true;
    CALDAV_FREE(url);
  }
  xml_node_free(root);
}

bool caldav_calendar_create_event(CalDAVCalendar *c, const char *uid, const char *ical) {
  if (!c || !uid || !ical) return false;
  return caldav_request_calendar_create_event(c, uid, ical);
}

bool caldav_calendar_update(CalDAVCalendar *c, const char *display_name, const char *description, const char *color) {
  if (!c) return false;
  bool res = caldav_request_calendar_update(c, display_name, description, color);
  if (!res) return false;
  char *response = caldav_request_calendar_get_props(c);
  if (!response) return false;
  XMLNode *root = xml_parse_string(response);
  CALDAV_FREE(response);
  if (!root) return false;
  XMLNode *multistatus = xml_node_child_at(root, 0);
  if (!multistatus) {
    xml_node_free(root);
    return false;
  }
  for (size_t i = 0; i < multistatus->children->len; ++i) {
    XMLNode *res = xml_node_child_at(multistatus, i);
    if (!res) continue;
    XMLNode *status = xml_node_find_tag(res, "status", false);
    if (!status || strcmp(status->text, "HTTP/1.1 200 OK") != 0) continue;
    XMLNode *href = xml_node_find_tag(res, "href", false);
    if (!href || !href->text || strcmp(c->href, href->text) != 0) continue;
    XMLNode *ctag = xml_node_find_tag(res, "propstat/prop/getctag", false);
    if (ctag && ctag->text && strcmp(ctag->text, c->ctag) != 0) CALDAV_REPLACE_STRING(c->ctag, ctag->text);
    XMLNode *display_name = xml_node_find_tag(res, "propstat/prop/displayname", false);
    if (display_name && display_name->text && strcmp(display_name->text, c->display_name) != 0)
      CALDAV_REPLACE_STRING(c->display_name, display_name->text);
    XMLNode *description = xml_node_find_tag(res, "propstat/prop/calendar-description", false);
    if (description && description->text && strcmp(description->text, c->description) != 0)
      CALDAV_REPLACE_STRING(c->description, description->text);
    XMLNode *color = xml_node_find_tag(res, "propstat/prop/calendar-color", false);
    if (color && color->text && strcmp(color->text, c->description) != 0)
      CALDAV_REPLACE_STRING(c->description, color->text);
  }
  xml_node_free(root);
  return true;
}

// ----------------------------------------------- //
//                      EVENT                      //
// ----------------------------------------------- //

static CalDAVEvent *caldav__event_new(CalDAVCalendar *c, const char *href, const char *ical, const char *etag) {
  if (!c) return NULL;
  CalDAVEvent *event = CALDAV_CALLOC_FUNC(1, sizeof(CalDAVEvent));
  event->calendar = c;
  event->href = href ? CALDAV_STRDUP_FUNC(href) : NULL;
  event->ical = ical ? CALDAV_STRDUP_FUNC(ical) : NULL;
  event->etag = etag ? CALDAV_STRDUP_FUNC(etag) : NULL;

  return event;
}

static void caldav__event_free(CalDAVEvent *e) {
  if (!e) return;
  CALDAV_FREE(e->href);
  CALDAV_FREE(e->ical);
  CALDAV_FREE(e);
}

void caldav_event_print(CalDAVEvent *e) {
  if (!e) return;
  printf("---------- EVENT ----------\n"
         "Href: %s\n"
         "ETag: %s\n"
         "Deleted: %s\n"
         "iCal:\n%s\n"
         "---------------------------\n",
         e->href, e->etag, e->deleted ? "true" : "false", e->ical);
}

bool caldav_event_delete(CalDAVEvent *e) {
  if (!e) return false;
  caldav__log("Deleting event: %s", e->href);
  bool res = false;
  caldav_request(e->calendar->client, e->href, "DELETE", NULL, NULL, &res);
  if (!res) return false;
  e->deleted = true;
  return true;
}

bool caldav_event_update(CalDAVEvent *e, const char *ical) {
  if (!e || !ical) return false;
  caldav__log("Updating event: %s", e->href);
  bool res = false;
  caldav_request(e->calendar->client, e->href, "PUT", NULL, ical, &res);
  if (!res) return false;
  CalDAVEvents evs = {0};
  da_add(&evs, e);
  char *response = caldav_request_calendar_multiget(e->calendar, &evs);
  da_free(&evs);
  if (!response) return false;
  XMLNode *root = xml_parse_string(response);
  CALDAV_FREE(response);
  if (!root) return false;
  XMLNode *multistatus = xml_node_child_at(root, 0);
  if (!multistatus) {
    xml_node_free(root);
    return false;
  }
  for (size_t i = 0; i < multistatus->children->len; ++i) {
    XMLNode *res = xml_node_child_at(multistatus, i);
    if (!res) continue;
    XMLNode *status = xml_node_find_tag(res, "status", false);
    if (!status || strcmp(status->text, "HTTP/1.1 200 OK") != 0) continue;
    XMLNode *href = xml_node_find_tag(res, "propstat/href", false);
    if (!href || !href->text || strcmp(e->href, href->text) != 0) continue;
    XMLNode *etag = xml_node_find_tag(res, "propstat/prop/getetag", false);
    if (!etag || !etag->text) continue;
    CALDAV_REPLACE_STRING(e->etag, etag->text);
    XMLNode *ical = xml_node_find_tag(res, "propstat/prop/calendar-data", false);
    if (!ical || !ical->text) continue;
    CALDAV_REPLACE_STRING(e->ical, ical->text);
  }
  xml_node_free(root);
  caldav__log("Updated successfully");
  return true;
}

#endif // CALDAV_IMPLEMENTATION
