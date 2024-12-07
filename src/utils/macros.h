#pragma once

// Log formatted message
#define LOG(format, ...) fprintf(stderr, "\033[0;32m[Errands] \033[0m" format "\n", ##__VA_ARGS__)
#define ERROR(format, ...)                                                                         \
  fprintf(stderr, "\033[1;31m[Errands Error] " format "\033[0m\n", ##__VA_ARGS__)

// For range
#define for_range(var, from, to) for (int var = from; var < to; var++)

// Avoid dangling pointers
// #define free(mem) \
//   free(mem); \ mem = NULL;

// Lambda function macro
// #define lambda(lambda$_ret, lambda$_args, lambda$_body) \
//   ({ \
//     lambda$_ret lambda$__anon$ lambda$_args\
// lambda$_body &lambda$__anon$; \
//   })
