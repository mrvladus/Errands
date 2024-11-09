#pragma once

#include "macros.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

static inline char *read_file_to_string(const char *path) {
  FILE *file = fopen(path, "r"); // Open the file in read mode
  if (!file) {
    LOG("Could not open file"); // Print error if file cannot be opened
    return NULL;
  }
  // Move the file pointer to the end of the file to get the size
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file); // Get the current position (file size)
  fseek(file, 0, SEEK_SET);     // Move back to the beginning of the file
  // Allocate memory for the string (+1 for the null terminator)
  char *buf = (char *)malloc(file_size + 1);
  fread(buf, 1, file_size, file);
  buf[file_size] = '\0'; // Null-terminate the string
  fclose(file);
  return buf;
}

static inline void write_string_to_file(const char *path, const char *str) {
  FILE *file = fopen(path, "w");
  if (file) {
    fprintf(file, "%s", str);
    fclose(file);
  }
}

static inline bool file_exists(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file) {
    fclose(file);
    return true;
  }
  return false;
}

static inline bool directory_exists(const char *path) {
  struct stat statbuf;
  return (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
}
