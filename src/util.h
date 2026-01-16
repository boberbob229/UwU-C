#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdbool.h>

// Memory allocation wrappers
void* xmalloc(size_t size);
void* xcalloc(size_t nmemb, size_t size);
void* xrealloc(void* ptr, size_t size);
char* xstrdup(const char* s);

// String utilities
bool str_eq(const char* a, const char* b);
char* read_file(const char* filename);

// Error handling
void error(const char* fmt, ...);
void error_at(int line, int col, const char* fmt, ...);
void warn_at(int line, int col, const char* fmt, ...);

#endif // UTIL_H
