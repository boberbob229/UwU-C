#define _POSIX_C_SOURCE 200809L
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Memory allocation wrappers with error checking

void* xmalloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        error("Out of memory");
    }
    return ptr;
}

void* xcalloc(size_t nmemb, size_t size) {
    void* ptr = calloc(nmemb, size);
    if (!ptr) {
        error("Out of memory");
    }
    return ptr;
}

void* xrealloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        error("Out of memory");
    }
    return new_ptr;
}

char* xstrdup(const char* s) {
    char* dup = strdup(s);
    if (!dup) {
        error("Out of memory");
    }
    return dup;
}

bool str_eq(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        error("Cannot open file: %s", filename);
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = xmalloc(size + 1);
    size_t read_size = fread(buffer, 1, size, f);
    buffer[read_size] = '\0';
    
    fclose(f);
    return buffer;
}

void error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void error_at(int line, int col, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "error at %d:%d: ", line, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void warn_at(int line, int col, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "warning at %d:%d: ", line, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
