#ifndef PUG_H
#define PUG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// ---------- INITIALIZATION ---------- //

// Initialize PUG. Call it first inside `main` function.
// Call if you want to parse command-line arguments and PUG could auto-rebuild its build script e.g. `pug.c`.
#define pug_init(argc, argv) pug__init(argc, argv, __FILE__)

// ---------- TARGET ---------- //

// Target object.
typedef struct _PugTarget PugTarget;

// Target type flags.
// PUG_TARGET_TYPE_STATIC_LIBRARY | PUG_TARGET_TYPE_SHARED_LIBRARY will build both static and shared libraries.
// PUG_TARGET_TYPE_EXECUTABLE | PUG_TARGET_TYPE_STATIC_LIBRARY | PUG_TARGET_TYPE_SHARED_LIBRARY will only build
// executable.
typedef enum {
  PUG_TARGET_TYPE_EXECUTABLE = 1 << 0,
  PUG_TARGET_TYPE_STATIC_LIBRARY = 1 << 1,
  PUG_TARGET_TYPE_SHARED_LIBRARY = 1 << 2,
} PugTargetType;

// Create a new target with the given name, type, and build directory.
PugTarget pug_target_new(const char *name, PugTargetType type, const char *build_dir);

// Add source file path to `target`.
void pug_target_add_source(PugTarget *target, const char *source);
// Add multiple sources to `target`. Convinience macro.
#define pug_target_add_sources(target_ptr, ...)                                                                        \
  for (const char *_s[] = {__VA_ARGS__, NULL}, **_p = _s; *_p; pug_target_add_source(target_ptr, *_p), _p++)

// Add C flag to `target`.
void pug_target_add_cflag(PugTarget *target, const char *cflag);
// Add cflags to `target`. Convinience macro.
#define pug_target_add_cflags(target_ptr, ...)                                                                         \
  for (const char *_s[] = {__VA_ARGS__, NULL}, **_p = _s; *_p; pug_target_add_cflag(target_ptr, *_p), _p++)

// Add linker flag to `target`.
void pug_target_add_ldflag(PugTarget *target, const char *ldflag);
// Add ldflags to `target`. Convinience macro.
#define pug_target_add_ldflags(target_ptr, ...)                                                                        \
  for (const char *_s[] = {__VA_ARGS__, NULL}, **_p = _s; *_p; pug_target_add_ldflag(target_ptr, *_p), _p++)

// Add pkg-config library name to `target`.
void pug_target_add_pkg_config_lib(PugTarget *target, const char *pkg_config_lib);
// Add multiple pkg-config libraries to `target`. Convinience macro.
#define pug_target_add_pkg_config_libs(target_ptr, ...)                                                                \
  for (const char *_s[] = {__VA_ARGS__, NULL}, **_p = _s; *_p; pug_target_add_pkg_config_lib(target_ptr, *_p), _p++)

// Build `target`
bool pug_target_build(PugTarget *target);

// ---------- UTILS ---------- //

// Check if `file1` is older than `file2`
bool pug_file1_is_older_than_file2(const char *file1, const char *file2);
// Check if `file` is older than any of files in NULL-terminated list of file paths
bool pug_file_is_older_than_files(const char *file, ...);
// Run formatted command. Returns `true` on success.
bool pug_cmd(const char *fmt, ...);
// Check if argument `arg` is present in command line arguments
bool pug_arg_bool(const char *arg);

// ---------- HELPFUL MACROS ---------- //

// Convert `define` to "-Ddefine".
// For example we have macro:
// #define ENABLE_FEATURE
// Using this will turn it into "-DENABLE_FEATURE".
// Use in pug_target_add_cflags().
#define PUG_CFLAG_FROM_DEFINE(define) "-D" #define

// Convert `define` to "-Ddefine='"<define value>"'".
// For example we have macro:
// #define VERSION "1.0"
// Using this will turn it into "-DVERSION='\"1.0\"'"
// Use in pug_target_add_cflags().
#define PUG_CFLAG_FROM_DEFINE_STR(define) "-D" #define "='\"" define "\"'"

// ---------- DEBUG ---------- //

#define pug_log(fmt, ...) fprintf(stderr, "[PUG] " fmt "\n", ##__VA_ARGS__)

#define pug_assert(condition)                                                                                          \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      pug_log("%s:%d:%s: ASSERT FAILED: " #condition, __FILE__, __LINE__, __func__);                                   \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while (0)

#define pug_assert_msg(condition, message)                                                                             \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      pug_log("%s:%d:%s: ASSERT FAILED: " #condition ". " message, __FILE__, __LINE__, __func__);                      \
      abort();                                                                                                         \
    }                                                                                                                  \
  } while (0)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // PUG_H

// ------------------------------ IMPLEMENTATION ------------------------------ //

#ifdef PUG_IMPLEMENTATION

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define stat   _stat
#define getcwd _getcwd
#else
#include <unistd.h>
#endif // _WIN32

// ---------- WINDOWS ---------- //

#ifdef _WIN32
// C compiler
#if defined(__clang__)
#define PUG_CC "clang"
#elif defined(__GNUC__)
#define PUG_CC "gcc"
#elif defined(_MSC_VER)
#define PUG_CC "cl.exe"
#endif
// C compiler options
#define PUG_OBJ_EXT           ".obj"
#define PUG_CC_SHARED_LIB_EXT ".dll"
#define PUG_CC_STATIC_LIB_EXT ".lib"
#define PUG_CC_EXE_EXT        ".exe"
// Commands
#define PUG_CC_AR_CMD_FORMAT "lib.exe /OUT:%s" PUG_CC_STATIC_LIB_EXT " %s"

// ---------- POSIX ---------- //

#else
// C compiler
#define PUG_CC                "cc"
// C compiler options
#define PUG_OBJ_EXT           "o"
#define PUG_CC_SHARED_LIB_EXT ".so"
#define PUG_CC_STATIC_LIB_EXT ".a"
#define PUG_CC_EXE_EXT        ""
// Commands
#define PUG_CC_AR_CMD_FORMAT  "ar rcs %s" PUG_CC_STATIC_LIB_EXT " %s"

#endif // _WIN32

// ---------- GLOBAL VARIABLES ---------- //

static int pug_argc;
static char **pug_argv;

// ---------- MEMORY BUFFER ---------- //

#ifndef PUG_MEM_SIZE
#define PUG_MEM_SIZE (1000 * 1000 * 4)
#endif

// Allocate memory with size `size` and memset it to 0
static void *pug__alloc(size_t size) {
  static char pug_mem[PUG_MEM_SIZE];
  static size_t pug_mem_offset = 0;
  pug_assert_msg(size > 0, "Can't allocate 0 bytes");
  pug_assert_msg(size <= PUG_MEM_SIZE - pug_mem_offset, "Internal memory buffer is full. Consider to redefine "
                                                        "PUG_MEM_SIZE to bigger value than (1000 * 1000 * 4) bytes.");
  void *ptr = pug_mem + pug_mem_offset;
  memset(ptr, 0, size);
  pug_mem_offset += size;

  return ptr;
}

static void *pug__realloc(void *ptr, size_t old_size, size_t new_size) {
  pug_assert(new_size != 0 && new_size >= old_size);
  if (!ptr) return pug__alloc(new_size);
  void *new_ptr = pug__alloc(new_size);
  size_t n = old_size < new_size ? old_size : new_size;
  memmove(new_ptr, ptr, n);

  return new_ptr;
}

// ---------- DYNAMIC ARRAY OF POINTERS ---------- //

typedef struct {
  void **data;
  size_t size;
  size_t capacity;
} PugArray;

static PugArray pug__array_init(size_t initial_capacity) {
  if (initial_capacity == 0) initial_capacity = 1;
  PugArray array = {0};
  array.data = pug__alloc(initial_capacity * sizeof(void *));
  if (!array.data) return array;
  array.capacity = initial_capacity;

  return array;
}

static bool pug__array_add(PugArray *array, void *value) {
  if (array->size == array->capacity) {
    size_t new_capacity = array->capacity * 2;
    void **new_data = pug__realloc(array->data, array->capacity * sizeof(void *), new_capacity * sizeof(void *));
    if (!new_data) return false;
    array->data = new_data;
    array->capacity = new_capacity;
  }
  array->data[array->size++] = value;

  return true;
}

// Convert array to string with separator
static const char *pug__array_to_string(PugArray *array, const char *separator) {
  if (!array || !array->data || array->size == 0) return NULL;
  size_t separator_len = separator ? strlen(separator) : 0;
  size_t len = 1;
  for (size_t i = 0; i < array->size; i++) len += strlen(array->data[i]) + separator_len;
  char *str = pug__alloc(len);
  for (size_t i = 0; i < array->size; i++) {
    strcat(str, array->data[i]);
    strcat(str, separator);
  }

  return str;
}

// ---------- STRING TOOLS ---------- //

// Internal helper: sprintf with va_list
static const char *pug__vsprintf(const char *format, va_list args) {
  pug_assert(format != NULL);
  va_list args_copy;
  va_copy(args_copy, args);
  int len = vsnprintf(NULL, 0, format, args_copy);
  va_end(args_copy);
  if (len < 0) return "";
  char *buf = pug__alloc((size_t)len + 1);
  vsnprintf(buf, (size_t)len + 1, format, args);

  return buf;
}

static const char *pug__sprintf(const char *format, ...) {
  pug_assert(format != NULL);
  va_list args;
  va_start(args, format);
  const char *res = pug__vsprintf(format, args);
  va_end(args);

  return res;
}

static const char *pug__replace_ext(const char *filename, const char *new_ext) {
  pug_assert(filename != NULL && new_ext != NULL);
  // Find the last dot and last slash
  const char *last_dot = strrchr(filename, '.');
  const char *last_slash = strrchr(filename, '/');
  // Handle paths with slashes - only consider dots after the last slash
  if (last_slash && last_dot && last_dot < last_slash) last_dot = NULL; // Dot is in directory path, not filename
  if (!last_dot) {
    // No valid extension found, append new extension
    const char *new_filename = pug__sprintf("%s.%s", filename, new_ext);
    return new_filename;
  }
  // Replace the extension
  size_t base_len = last_dot - filename;
  const char *new_filename = pug__sprintf("%.*s.%s", (int)base_len, filename, new_ext);

  return new_filename;
}

static const char *pug__replace_char(const char *string, char to_replace, char replace_with) {
  pug_assert(string != NULL);
  char *result = pug__alloc(strlen(string) + 1);
  strcpy(result, string);
  for (char *p = result; *p != '\0'; p++)
    if (*p == to_replace) *p = replace_with;

  return result;
}

// ---------- TARGET ---------- //

struct _PugTarget {
  const char *name;
  PugTargetType type;
  const char *build_dir;
  PugArray sources;
  PugArray cflags;
  PugArray ldflags;
  PugArray pkg_config_libs;

  PugArray objects;
};

PugTarget pug_target_new(const char *name, PugTargetType type, const char *build_dir) {
  PugTarget target = {0};
  target.name = name;
  target.type = type;
  target.build_dir = build_dir;
  target.sources = pug__array_init(16);
  target.cflags = pug__array_init(16);
  target.ldflags = pug__array_init(16);
  target.pkg_config_libs = pug__array_init(16);
  target.objects = pug__array_init(16);

  return target;
}

#define PUG__TARGET_ADD_FUNC_IMPL(arr, arg)                                                                            \
  void pug_target_add_##arg(PugTarget *target, const char *arg) { pug__array_add(&target->arr, (void *)arg); }

PUG__TARGET_ADD_FUNC_IMPL(sources, source);
PUG__TARGET_ADD_FUNC_IMPL(cflags, cflag);
PUG__TARGET_ADD_FUNC_IMPL(ldflags, ldflag);
PUG__TARGET_ADD_FUNC_IMPL(pkg_config_libs, pkg_config_lib);

// ---------- CMD TOOLS ---------- //

bool pug_cmd(const char *fmt, ...) {
  pug_assert_msg(fmt != NULL, "Command cannot be NULL");
  va_list args;
  va_start(args, fmt);
  const char *cmd = pug__vsprintf(fmt, args);
  va_end(args);
  pug_log("%s", cmd);
  int res = system(cmd);

  return 0 == res;
}

// ---------- FILE TOOLS ---------- //

static bool pug__mkdir(const char *path) {
  pug_assert(path != NULL);
#ifdef _WIN32
  bool res = _mkdir(path) == 0;
#else
  bool res = mkdir(path, 0755) == 0;
#endif
  return res;
}

static bool pug__dir_exists(const char *path) {
  pug_assert(path != NULL);
  struct stat statbuf;
  if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) return true;

  return false;
}

static bool pug__file_exists(const char *path) {
  pug_assert(path != NULL);
  FILE *fp = fopen(path, "r");
  if (!fp) return false;
  fclose(fp);

  return true;
}

bool pug_file1_is_older_than_file2(const char *file1, const char *file2) {
  if (!file1 || !file2) return false;
  struct stat stat1, stat2;
  if (stat(file1, &stat1) != 0 || stat(file2, &stat2) != 0) return false;

  return stat1.st_mtime < stat2.st_mtime;
}

bool pug_file_is_older_than_files(const char *file, ...) {
  pug_assert(file != NULL);
  va_list args;
  va_start(args, file);
  bool changed = false;
  const char *file_to_check = va_arg(args, const char *);
  while (file_to_check) {
    changed |= pug_file1_is_older_than_file2(file, file_to_check);
    file_to_check = va_arg(args, const char *);
  }
  va_end(args);

  return changed;
}

static const char *pug__basename(const char *path) {
  pug_assert(path != NULL);
  const char *base = strrchr(path, '/');
  if (!base) return path;

  return base + 1;
}

static const char *pug__dirname(const char *path) {
  pug_assert(path != NULL);
  const char *last_slash = strrchr(path, '/');
  size_t len = last_slash ? (size_t)(last_slash - path) : 0;
  char *dirname = pug__alloc(len + 1);
  if (len) memcpy(dirname, path, len);
  dirname[len] = '\0';

  return dirname ? dirname : ".";
}

static const char *pug__cwd() {
  char *tmp = getcwd(NULL, 0);
  if (!tmp) return NULL;
  const char *cwd = pug__sprintf("%s", tmp);
  free(tmp);

  return cwd;
}

// ---------- PARSING ---------- //

static PugArray pug__find_headers(const char *source_file_path) {
  pug_assert(source_file_path != NULL);
  FILE *file = fopen(source_file_path, "r");
  PugArray arr = pug__array_init(8);
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  while ((read = getline(&line, &len, file)) != -1) {
    char *ptr = line;
    while (isspace(*ptr)) ptr++;
    if (strncmp(ptr, "#include", 8) == 0) {
      ptr += 8;
      while (isspace(*ptr)) ptr++;
      if (*ptr == '"') {
        ptr++;
        char *end_quote = strchr(ptr, '"');
        if (end_quote) {
          *end_quote = '\0'; // Temporarily null-terminate
          const char *header_path = pug__sprintf("%s/%s", pug__dirname(source_file_path), ptr);
          *end_quote = '"'; // Restore the quote
          if (pug__file_exists(header_path)) pug__array_add(&arr, (void *)header_path);
        }
      }
    }
  }
  free(line);
  fclose(file);

  return arr;
}

// ---------- INITIALIZATION ---------- //

// Initialize PUG and rebuild itself if needed
static void pug__init(int argc, char **argv, const char *build_file_path) {
  pug_argc = argc;
  pug_argv = argv;
  if (pug_file1_is_older_than_file2(argv[0], build_file_path)) {
    pug_log("%s -> %s", build_file_path, argv[0]);
    bool res = pug_cmd(PUG_CC " %s -o %s", build_file_path, argv[0]);
    if (!res) exit(EXIT_FAILURE);
    execv(argv[0], argv);
  }
}

// ---------- COMMAND LINE PARSING ---------- //

bool pug_arg_bool(const char *arg) {
  pug_assert_msg(pug_argc > 0 && pug_argv != NULL,
                 "Can't parse arguments. Did you forget to call pug_init(argc, argv)?");
  for (size_t i = 1; i < pug_argc; i++)
    if (strcmp(pug_argv[i], arg) == 0) return true;

  return false;
}

// ---------- BUILD ---------- //

static bool pug__build_object_files(PugTarget *target, bool *need_linking) {
  for (size_t i = 0; i < target->sources.size; i++) {
    const char *source_file = target->sources.data[i];
    // Convert path/to/source.c -> path_to_source.c to avoid collisions
    const char *source_file_mangled = pug__replace_char(source_file, '/', '_');
    const char *obj_file = pug__sprintf("%s/%s", target->build_dir, pug__replace_ext(source_file_mangled, "o"));
    pug__array_add(&target->objects, (void *)obj_file);
    // Check if source file needs building
    bool source_needs_build = false;
    if (!pug__file_exists(obj_file)) source_needs_build = true;
    if (pug_file1_is_older_than_file2(obj_file, source_file)) source_needs_build = true;
    // Check for changed headers
    PugArray headers = pug__find_headers(source_file);
    for (size_t i = 0; i < headers.size; i++)
      if (pug_file1_is_older_than_file2(obj_file, headers.data[i])) source_needs_build = true;
    // Build obj file if needed
    if (source_needs_build) {
      *need_linking = true;
      pug_log("%s -> %s", source_file, obj_file);
      const char *cflags = pug__array_to_string(&target->cflags, " ");
      const char *pkg_config_libs = pug__array_to_string(&target->pkg_config_libs, " ");
      // Add pkg-config cflags if needed
      const char *pkg_config_flags = "";
#ifndef _WIN32 // Skip pkg-config on Windows
      if (pkg_config_libs) pkg_config_flags = pug__sprintf("$(pkg-config --cflags %s)", pkg_config_libs);
#endif
      // Run command
      bool res = pug_cmd(PUG_CC " -c %s -o %s %s %s", source_file, obj_file, cflags, pkg_config_flags);
      if (!res) return false;
    }
  }

  return true;
}

static bool pug__link_object_files(PugTarget *target) {
  const char *path = pug__sprintf("%s/%s", target->build_dir, target->name);
  const char *obj_str = pug__array_to_string(&target->objects, " ");
  const char *pkg_config_libs = pug__array_to_string(&target->pkg_config_libs, " ");
  const char *ldflags = pug__array_to_string(&target->ldflags, " ");
  // Add pkg-config ldflags if needed
  const char *pkg_config_flags = "";
#ifndef _WIN32 // Skip pkg-config on Windows
  if (pkg_config_libs) pkg_config_flags = pug__sprintf("$(pkg-config --libs %s)", pkg_config_libs);
#endif
  // Link executable
  if (target->type & PUG_TARGET_TYPE_EXECUTABLE) {
    pug_log("Linking executable -> %s", path);
    bool res =
        pug_cmd(PUG_CC " %s" PUG_CC_EXE_EXT " -o %s %s %s", obj_str, path, ldflags ? ldflags : "", pkg_config_flags);
    if (!res) return false;
  } else {
    // Link static library
    if (target->type & PUG_TARGET_TYPE_STATIC_LIBRARY) {
      pug_log("Linking static library -> %s", path);
      bool res = pug_cmd(PUG_CC_AR_CMD_FORMAT, path, obj_str);
      if (!res) return false;
    }
    // Link dynamic library
    if (target->type & PUG_TARGET_TYPE_SHARED_LIBRARY) {
      pug_log("Linking dynamic library -> %s", path);
      bool res = pug_cmd(PUG_CC " -shared %s -o %s" PUG_CC_SHARED_LIB_EXT " %s %s", obj_str, path,
                         ldflags ? ldflags : "", pkg_config_flags);
      if (!res) return false;
    }
  }

  return true;
}

// Build target
bool pug_target_build(PugTarget *target) {
  pug_assert_msg(target != NULL, "Can't build empty target");
  pug_assert_msg(target->build_dir != NULL, "Build directory is not set");
  // Create build directory
  if (!pug__dir_exists(target->build_dir)) {
    pug_log("Creating build directory '%s'", target->build_dir);
    pug__mkdir(target->build_dir);
  }
  pug_log("Building target '%s'", target->name);
  // Build
  bool needs_linking = false;
  if (!pug__build_object_files(target, &needs_linking)) return false;
  if (needs_linking)
    if (!pug__link_object_files(target)) return false;

  return true;
}

#endif // PUG_IMPLEMENTATION
