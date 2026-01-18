#ifndef UWU_STDLIB_H
#define UWU_STDLIB_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void* uwu_malloc(size_t size);
void* uwu_calloc(size_t count, size_t size);
void* uwu_realloc(void* ptr, size_t new_size);
void  uwu_free(void* ptr);
void  uwu_report_leaks(void);

void  uwu_print(const char* str);
void  uwu_printf(const char* format, ...);
int   uwu_scanf(const char* format, ...);

FILE* uwu_fopen(const char* filename, const char* mode);
int   uwu_fclose(FILE* stream);
size_t uwu_fread(void* ptr, size_t size, size_t count, FILE* stream);
size_t uwu_fwrite(const void* ptr, size_t size, size_t count, FILE* stream);

int   uwu_strlen(const char* str);
char* uwu_strcpy(char* dest, const char* src);
char* uwu_strncpy(char* dest, const char* src, int n);
char* uwu_strcat(char* dest, const char* src);
char* uwu_strncat(char* dest, const char* src, int n);
int   uwu_strcmp(const char* str1, const char* str2);
int   uwu_strncmp(const char* str1, const char* str2, int n);
char* uwu_strchr(const char* str, int ch);
char* uwu_strrchr(const char* str, int ch);
char* uwu_strdup(const char* str);

double uwu_sqrt(double x);
double uwu_pow(double base, double exp);
int    uwu_abs(int x);
double uwu_fabs(double x);
double uwu_sin(double x);
double uwu_cos(double x);
double uwu_tan(double x);
double uwu_asin(double x);
double uwu_acos(double x);
double uwu_atan(double x);
double uwu_atan2(double y, double x);
double uwu_log(double x);
double uwu_log10(double x);
double uwu_exp(double x);
double uwu_floor(double x);
double uwu_ceil(double x);
double uwu_round(double x);

void  uwu_exit(int code);
void  uwu_abort(void);
int   uwu_rand(void);
void  uwu_srand(int seed);
long  uwu_time(void);
void  uwu_sleep(int milliseconds);

void  uwu_bounds_error(const char* message);
void  uwu_null_error(const char* message);
void  uwu_stack_overflow(void);
void  uwu_check_bounds(int index, int size, const char* file, int line);
void  uwu_check_null(void* ptr, const char* file, int line);

void* uwu_memcpy(void* dest, const void* src, size_t n);
void* uwu_memmove(void* dest, const void* src, size_t n);
void* uwu_memset(void* ptr, int value, size_t n);
int   uwu_memcmp(const void* ptr1, const void* ptr2, size_t n);

void  uwu_init(void);
void  uwu_cleanup(void);

#define UWU_MALLOC(type) ((type*)uwu_malloc(sizeof(type)))
#define UWU_MALLOC_ARRAY(type, count) ((type*)uwu_malloc(sizeof(type) * (count)))
#define UWU_CALLOC(type, count) ((type*)uwu_calloc((count), sizeof(type)))

#ifdef __cplusplus
}
#endif

#endif
