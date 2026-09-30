#pragma once

#include "data.h"

#include <ctype.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <libical/ical.h>

#define STR_TO_BOOL(str)     (g_str_equal((str), "1") || g_str_equal((str), "true"))
#define BOOL_TO_STR_NUM(val) ((val) ? "1" : "0")

// Print TODO formatted message.
#define TODO(format, ...) fprintf(stderr, "%s:%d:%s: " format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define for_range(idx, idx_start, idx_end)  for (int idx = (int)(idx_start); idx < (int)(idx_end); ++idx)
#define for_item_in_gptrarray(T, item, arr) for (T *item = (T *)arr->pdata; item < arr->pdata + arr->len; item++)
#define CONTINUE_IF(statement)                                                                                                   \
  if (statement) continue
#define CONTINUE_IF_NOT(statement)                                                                                               \
  if (!(statement)) continue

// Get current time
#define TIME_NOW time(NULL)
// Start timer
#define TIMER_START clock_t __timer_start = clock();
// Get elapsed time in milliseconds
#define TIMER_ELAPSED_MS ((double)(clock() - __timer_start) / CLOCKS_PER_SEC)
// Generate random seed
#define RANDOM_SEED() srand((unsigned int)(TIME_NOW ^ getpid()));

// This is temporary buffer for printing formatted strings.
// If you need to get formatted string but not wanting to use malloc, use this.
// Default size is 8192 bytes. You can change it by defining TB_TMP_STR_BUFFER_SIZE.
// When buffer is full - starts overriding buffer from the beginning.
static inline const char *tmp_str_printf(const char *format, ...) {
#define BUFFER_SIZE 4096

  static char buffer[BUFFER_SIZE];
  static size_t offset = 0;

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

  if ((size_t)len >= BUFFER_SIZE - offset - 1) offset = 0;
  // Print the string
  vsnprintf(buffer + offset, BUFFER_SIZE - offset, format, args);
  const char *result = buffer + offset;
  offset += len + 1;
  va_end(args);

  return result;

#undef BUFFER_SIZE
}

static inline const char *generate_hex_as_str() {
  static char hex[8] = {0};
  sprintf(hex, "#%06x", rand() % 0xFFFFFF);
  return hex;
}

static inline int cmd_run_stdout(const char *cmd, char **std_out) {
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

// Get children of the widget
static inline GPtrArray *get_children(GtkWidget *parent) {
  GPtrArray *children = g_ptr_array_new();
  GtkWidget *first_child = gtk_widget_get_first_child(parent);
  if (first_child) {
    g_ptr_array_add(children, first_child);
    GtkWidget *next_child = gtk_widget_get_next_sibling(first_child);
    while (next_child) {
      g_ptr_array_add(children, next_child);
      next_child = gtk_widget_get_next_sibling(next_child);
    }
  }
  return children;
}

static inline void get_children_to_array(GtkWidget *parent, GPtrArray *children) {
  GtkWidget *first_child = gtk_widget_get_first_child(parent);
  if (first_child) {
    g_ptr_array_add(children, first_child);
    GtkWidget *next_child = gtk_widget_get_next_sibling(first_child);
    while (next_child) {
      g_ptr_array_add(children, next_child);
      next_child = gtk_widget_get_next_sibling(next_child);
    }
  }
}

static inline void get_children_sized(GtkWidget *parent, GtkWidget **children, size_t n) {
  GtkWidget *first_child = gtk_widget_get_first_child(parent);
  if (first_child) {
    children[0] = first_child;
    GtkWidget *next_child = gtk_widget_get_next_sibling(first_child);
    for (size_t i = 1; i < n && next_child; i++) {
      children[i] = next_child;
      next_child = gtk_widget_get_next_sibling(next_child);
    }
  }
}

#define for_child_in_parent(child, parent)                                                                                       \
  for (GtkWidget *child = gtk_widget_get_first_child(parent); child; child = gtk_widget_get_next_sibling(child))

static inline void gtk_box_remove_all(GtkWidget *box) {
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child(box))) gtk_box_remove(GTK_BOX(box), child);
}

// Function to trim leading and trailing whitespace
static inline char *string_trim(char *str) {
  // Trim leading whitespace
  while (isspace((unsigned char)*str)) str++;
  // Trim trailing whitespace
  char *end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) end--;
  // Null terminate the trimmed string
  *(end + 1) = '\0';
  return str;
}

static inline void gdk_rgba_to_hex_string(const GdkRGBA *rgba, char hex_string[8]) {
  // Convert the RGBA components to integers in the range [0, 255]
  int r = (int)(rgba->red * 255);
  int g = (int)(rgba->green * 255);
  int b = (int)(rgba->blue * 255);
  // Format the string as #RRGGBB
  sprintf(hex_string, "#%02X%02X%02X", r, g, b);
}

// Add shortcut to the widget
static inline void errands_add_shortcut(GtkWidget *widget, const char *trigger, const char *action) {
  GtkEventController *ctrl = gtk_shortcut_controller_new();
  gtk_shortcut_controller_set_scope(GTK_SHORTCUT_CONTROLLER(ctrl), GTK_SHORTCUT_SCOPE_GLOBAL);
  GtkShortcut *sc = gtk_shortcut_new(gtk_shortcut_trigger_parse_string(trigger), gtk_shortcut_action_parse_string(action));
  gtk_shortcut_controller_add_shortcut(GTK_SHORTCUT_CONTROLLER(ctrl), sc);
  gtk_widget_add_controller(widget, ctrl);
}

static inline GStrv gstrv_remove_duplicates(GStrv strv) {
  g_autoptr(GHashTable) hash_table = g_hash_table_new(g_str_hash, g_str_equal);
  g_autoptr(GStrvBuilder) builder = g_strv_builder_new();
  for (int i = 0; strv[i] != NULL; i++)
    if (!g_hash_table_contains(hash_table, strv[i])) {
      g_hash_table_add(hash_table, strv[i]);
      g_strv_builder_add(builder, strv[i]);
    }
  return g_strv_builder_end(builder);
}

static inline GSimpleActionGroup *errands_add_action_group(void *widget, const char *group_name) {
  g_autoptr(GSimpleActionGroup) ag = g_simple_action_group_new();
  gtk_widget_insert_action_group(GTK_WIDGET(widget), group_name, G_ACTION_GROUP(ag));
  return ag;
}

static inline void errands_add_action(GSimpleActionGroup *ag, const char *name, void *cb, void *data, const char *param_str) {
  g_autoptr(GVariantType) vtype = param_str ? g_variant_type_new(param_str) : NULL;
  g_autoptr(GSimpleAction) action = g_simple_action_new(name, vtype);
  g_signal_connect(action, "activate", G_CALLBACK(cb), data);
  g_action_map_add_action(G_ACTION_MAP(ag), G_ACTION(action));
}

// Adds a stateful action to the action group.
// Callback is called when the state changes.
// Its type is `void (*cb)(GSimpleAction *action, GVariant *value, gpointer cb_data)`
static inline void errands_add_stateful_action(GSimpleActionGroup *ag, const char *name, const GVariantType *param_type,
                                               GVariant *initial_state, void *cb, void *cb_data) {
  g_autoptr(GSimpleAction) action = g_simple_action_new_stateful(name, param_type, initial_state);
  g_signal_connect(action, "change-state", G_CALLBACK(cb), cb_data);
  g_action_map_add_action(G_ACTION_MAP(ag), G_ACTION(action));
}

static inline gchar *str_to_markup(const char *str) {
  if (!str) return NULL;

  g_autoptr(GError) error = NULL;
  g_autofree gchar *escaped_text = g_markup_escape_text(str, -1);
  const char *pattern = "(http[s]?://[\\w\\-\\.]+(:\\d+)?(/\\S*)?)";
  g_autoptr(GRegex) regex = g_regex_new(pattern, G_REGEX_CASELESS, 0, &error);
  if (error) return NULL;
  gchar *markup = g_regex_replace(regex, escaped_text, -1, 0, "<a href=\"\\0\">\\0</a>", 0, &error);
  if (error) return NULL;

  return markup;
}

static inline void gtk_widget_set_color(GtkWidget *widget, const char *color) {
  g_assert_nonnull(widget);

  // Remove any existing custom color classes
  g_auto(GStrv) classes = gtk_widget_get_css_classes(widget);
  for (int i = 0; classes[i]; i++)
    if (g_str_has_prefix(classes[i], "custom-color-")) gtk_widget_remove_css_class(widget, classes[i]);

  // Remove previously-installed provider for this widget
  GtkCssProvider *old_provider = g_object_get_data(G_OBJECT(widget), "custom-color-provider");
  if (old_provider) {
    gtk_style_context_remove_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(old_provider));
    g_object_set_data(G_OBJECT(widget), "custom-color-provider", NULL);
  }

  if (!color) return;

  // Parse color string
  GdkRGBA rgba = {0};
  gboolean res = gdk_rgba_parse(&rgba, color);
  if (!res) return;
  char bg[8];
  gdk_rgba_to_hex_string(&rgba, bg);
  g_autofree gchar *css_class = g_strdup_printf("custom-color-%s", bg + 1);
  gtk_widget_add_css_class(widget, css_class);

  // Determine contrasting foreground color usinng YIQ formula
  double brightness = (rgba.red * 0.299) + (rgba.green * 0.587) + (rgba.blue * 0.114);
  const char *fg = brightness < 0.6 ? "white" : "black";

  // Build CSS rule and load it into a fresh provider
  const char *css_fmt = ".%s { background-color: %s; color: %s; }";
  g_autofree gchar *css = g_strdup_printf(css_fmt, css_class, bg, fg);
  g_autoptr(GtkCssProvider) provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(provider, css);
  gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_set_data(G_OBJECT(widget), "custom-color-provider", provider);
}

// ---------- ICAL UTILS ---------- //

#define for_vtodo_in_vcalendar(var, calendar)                                                                                    \
  for (icalcomponent *var = icalcomponent_get_first_component((calendar), ICAL_VTODO_COMPONENT); var;                            \
       var = icalcomponent_get_next_component((calendar), ICAL_VTODO_COMPONENT))

static inline GPtrArray *vcalendar_to_array(icalcomponent *calendar) {
  g_assert(icalcomponent_isa(calendar) == ICAL_VCALENDAR_COMPONENT);
  GPtrArray *array = g_ptr_array_new();
  for_vtodo_in_vcalendar(var, calendar) g_ptr_array_add(array, var);
  return array;
}

static inline icalcomponent *find_vtodo_by_uid(icalcomponent *ical, const char *uid) {
  icalcomponent *vtodo = icalcomponent_get_first_component(ical, ICAL_VTODO_COMPONENT);
  while (vtodo) {
    if (g_str_equal(uid, errands_data_get_uid(vtodo))) return vtodo;
    vtodo = icalcomponent_get_next_component(vtodo, ICAL_VTODO_COMPONENT);
  }
  return NULL;
}

static inline bool icaltime_is_null_date(const struct icaltimetype t) { return t.year == 0 && t.month == 0 && t.day == 0; }

static inline icaltimetype icaltime_merge_date_and_time(const struct icaltimetype date, const struct icaltimetype time) {
  icaltimetype result = date;
  result.hour = time.hour;
  result.minute = time.minute;
  result.second = time.second;
  result.is_date = false;
  return result;
}

static inline icaltimetype icaltime_get_date_time_now() {
  g_autoptr(GDateTime) dt = g_date_time_new_now_local();
  icaltimetype dt_now = icaltime_today();
  dt_now.is_date = false;
  dt_now.hour = g_date_time_get_hour(dt);
  dt_now.minute = g_date_time_get_minute(dt);
  dt_now.second = g_date_time_get_second(dt);
  return dt_now;
}

static inline bool icalrecurrencetype_compare(const struct icalrecurrencetype *a, const struct icalrecurrencetype *b) {
  if (!a || !b) return false;
  return memcmp(a, b, sizeof(*a)) == 0;
}
