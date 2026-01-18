#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#define UWU_MAX_ALLOCS 10000
#define UWU_ENABLE_LEAK_DETECTION 1
#define UWU_ENABLE_DOUBLE_FREE_DETECTION 1

typedef struct {
    void* ptr;
    size_t size;
    bool freed;
} UwuAllocation;

static UwuAllocation uwu_allocs[UWU_MAX_ALLOCS];
static size_t uwu_alloc_count;
static size_t uwu_total_allocated;
static size_t uwu_total_freed;

static void uwu_track_alloc(void* ptr, size_t size) {
    if (uwu_alloc_count >= UWU_MAX_ALLOCS) {
        fprintf(stderr, "allocation tracker overflow\n");
        abort();
    }

    uwu_allocs[uwu_alloc_count++] = (UwuAllocation){
        .ptr = ptr,
        .size = size,
        .freed = false
    };

    uwu_total_allocated += size;
}

static void uwu_track_free(void* ptr) {
    for (size_t i = 0; i < uwu_alloc_count; i++) {
        if (uwu_allocs[i].ptr == ptr) {
            if (uwu_allocs[i].freed) {
                fprintf(stderr, "double free detected\n");
                abort();
            }
            uwu_allocs[i].freed = true;
            uwu_total_freed += uwu_allocs[i].size;
            return;
        }
    }
}

void* uwu_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "malloc failed\n");
        return NULL;
    }
#if UWU_ENABLE_LEAK_DETECTION
    uwu_track_alloc(ptr, size);
#endif
    return ptr;
}

void* uwu_calloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (!ptr) {
        fprintf(stderr, "calloc failed\n");
        return NULL;
    }
#if UWU_ENABLE_LEAK_DETECTION
    uwu_track_alloc(ptr, count * size);
#endif
    return ptr;
}

void* uwu_realloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "realloc failed\n");
        return NULL;
    }
#if UWU_ENABLE_LEAK_DETECTION
    if (ptr) uwu_track_free(ptr);
    if (new_ptr) uwu_track_alloc(new_ptr, size);
#endif
    return new_ptr;
}

void uwu_free(void* ptr) {
    if (!ptr) return;
#if UWU_ENABLE_DOUBLE_FREE_DETECTION
    uwu_track_free(ptr);
#endif
    free(ptr);
}

void uwu_report_leaks(void) {
#if UWU_ENABLE_LEAK_DETECTION
    for (size_t i = 0; i < uwu_alloc_count; i++) {
        if (!uwu_allocs[i].freed) {
            fprintf(stderr, "leak: %zu bytes at %p\n",
                    uwu_allocs[i].size,
                    uwu_allocs[i].ptr);
        }
    }
#endif
}

void print_str(const char* str) {
    if (str) puts(str);
}

int read_int(void) {
    int n;
    if (scanf("%d", &n) != 1) {
        return 0;
    }
    return n;
}

void print_int(int n) {
    printf("%d\n", n);
}

void uwu_print(const char* str) {
    if (str) fputs(str, stdout);
}

void uwu_printf(const char* fmt, ...) {
    if (!fmt) return;
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

int uwu_scanf(const char* fmt, ...) {
    if (!fmt) return -1;
    va_list args;
    va_start(args, fmt);
    int r = vscanf(fmt, args);
    va_end(args);
    return r;
}

FILE* uwu_fopen(const char* path, const char* mode) {
    if (!path || !mode) return NULL;
    return fopen(path, mode);
}

int uwu_fclose(FILE* f) {
    return f ? fclose(f) : -1;
}

size_t uwu_fread(void* ptr, size_t size, size_t count, FILE* f) {
    return (ptr && f) ? fread(ptr, size, count, f) : 0;
}

size_t uwu_fwrite(const void* ptr, size_t size, size_t count, FILE* f) {
    return (ptr && f) ? fwrite(ptr, size, count, f) : 0;
}

int uwu_strlen(const char* s) {
    return s ? (int)strlen(s) : 0;
}

char* uwu_strcpy(char* d, const char* s) {
    return (d && s) ? strcpy(d, s) : d;
}

char* uwu_strncpy(char* d, const char* s, int n) {
    return (d && s) ? strncpy(d, s, n) : d;
}

char* uwu_strcat(char* d, const char* s) {
    return (d && s) ? strcat(d, s) : d;
}

char* uwu_strncat(char* d, const char* s, int n) {
    return (d && s) ? strncat(d, s, n) : d;
}

int uwu_strcmp(const char* a, const char* b) {
    return (a && b) ? strcmp(a, b) : 0;
}

int uwu_strncmp(const char* a, const char* b, int n) {
    return (a && b) ? strncmp(a, b, n) : 0;
}

char* uwu_strchr(const char* s, int c) {
    return s ? strchr(s, c) : NULL;
}

char* uwu_strrchr(const char* s, int c) {
    return s ? strrchr(s, c) : NULL;
}

char* uwu_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* r = uwu_malloc(len);
    if (r) memcpy(r, s, len);
    return r;
}

double uwu_sqrt(double x) { return x < 0 ? 0.0 : sqrt(x); }
double uwu_pow(double b, double e) { return pow(b, e); }
int uwu_abs(int x) { return abs(x); }
double uwu_fabs(double x) { return fabs(x); }
double uwu_sin(double x) { return sin(x); }
double uwu_cos(double x) { return cos(x); }
double uwu_tan(double x) { return tan(x); }
double uwu_asin(double x) { return asin(x); }
double uwu_acos(double x) { return acos(x); }
double uwu_atan(double x) { return atan(x); }
double uwu_atan2(double y, double x) { return atan2(y, x); }
double uwu_log(double x) { return x <= 0 ? 0.0 : log(x); }
double uwu_log10(double x) { return x <= 0 ? 0.0 : log10(x); }
double uwu_exp(double x) { return exp(x); }
double uwu_floor(double x) { return floor(x); }
double uwu_ceil(double x) { return ceil(x); }
double uwu_round(double x) { return round(x); }

void uwu_exit(int code) {
    uwu_report_leaks();
    exit(code);
}

void uwu_abort(void) {
    uwu_report_leaks();
    abort();
}

int uwu_rand(void) { return rand(); }
void uwu_srand(int seed) { srand(seed); }
long uwu_time(void) { return (long)time(NULL); }

void uwu_sleep(int ms) {
    clock_t start = clock();
    while ((clock() - start) * 1000 / CLOCKS_PER_SEC < ms) {}
}

void uwu_bounds_error(const char* msg) {
    if (msg) fputs(msg, stderr);
    uwu_report_leaks();
    abort();
}

void uwu_null_error(const char* msg) {
    if (msg) fputs(msg, stderr);
    uwu_report_leaks();
    abort();
}

void uwu_stack_overflow(void) {
    uwu_report_leaks();
    abort();
}

void uwu_check_bounds(int i, int n, const char* f, int l) {
    (void)f;
    (void)l;
    if (i < 0 || i >= n) uwu_bounds_error("out of bounds\n");
}

void uwu_check_null(void* p, const char* f, int l) {
    (void)f;
    (void)l;
    if (!p) uwu_null_error("null pointer\n");
}

void* uwu_memcpy(void* d, const void* s, size_t n) {
    return (d && s) ? memcpy(d, s, n) : d;
}

void* uwu_memmove(void* d, const void* s, size_t n) {
    return (d && s) ? memmove(d, s, n) : d;
}

void* uwu_memset(void* p, int v, size_t n) {
    return p ? memset(p, v, n) : p;
}

int uwu_memcmp(const void* a, const void* b, size_t n) {
    return (a && b) ? memcmp(a, b, n) : 0;
}

void uwu_init(void) {
    srand((unsigned)time(NULL));
    memset(uwu_allocs, 0, sizeof(uwu_allocs));
    uwu_alloc_count = 0;
    uwu_total_allocated = 0;
    uwu_total_freed = 0;
}

void uwu_cleanup(void) {
    uwu_report_leaks();
}