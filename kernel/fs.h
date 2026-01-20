#ifndef FS_H
#define FS_H

#include "types.h"

#define FS_MAX_FILES 64
#define FS_MAX_FILENAME 64
#define FS_MAX_FILESIZE 8192 // Increased for larger packages
#define FS_MAGIC 0x55575546

typedef struct {
    char name[FS_MAX_FILENAME];
    u32 size;
    u32 offset;
    u8 is_directory;
    u8 in_use;
} fs_file_t;

typedef struct {
    u32 magic;
    u32 num_files;
    fs_file_t files[FS_MAX_FILES];
    u8 data[FS_MAX_FILES * FS_MAX_FILESIZE];
} fs_t;

void fs_init(void);
int fs_create(const char* path, u8 is_directory);
int fs_write(const char* path, const u8* data, u32 size);
int fs_read(const char* path, u8* data, u32 max_size);
int fs_delete(const char* path);
int fs_list(const char* path, char* buffer, u32 max_size);
int fs_exists(const char* path);
u32 fs_size(const char* path);

#endif
