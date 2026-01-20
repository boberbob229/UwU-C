#include "fs.h"

extern void terminal_writeln(const char* s);

static fs_t filesystem;
static u32 next_data_offset = 0;

static void str_copy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

static int str_compare(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *(const u8*)a - *(const u8*)b;
}

static u32 str_len(const char* s) {
    u32 len = 0;
    while (*s++) len++;
    return len;
}

void fs_init(void) {
    filesystem.magic = FS_MAGIC;
    filesystem.num_files = 0;
    next_data_offset = 0;
    
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        filesystem.files[i].in_use = 0;
        filesystem.files[i].size = 0;
        filesystem.files[i].offset = 0;
        filesystem.files[i].is_directory = 0;
    }
    
    fs_create("/", 1);
}

int fs_create(const char* path, u8 is_directory) {
    if (filesystem.num_files >= FS_MAX_FILES) {
        return -1;
    }
    
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (filesystem.files[i].in_use && !str_compare(filesystem.files[i].name, path)) {
            return -2;
        }
    }
    
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (!filesystem.files[i].in_use) {
            str_copy(filesystem.files[i].name, path);
            filesystem.files[i].size = 0;
            filesystem.files[i].offset = next_data_offset;
            filesystem.files[i].is_directory = is_directory;
            filesystem.files[i].in_use = 1;
            filesystem.num_files++;
            return 0;
        }
    }
    
    return -3;
}

int fs_write(const char* path, const u8* data, u32 size) {
    if (size > FS_MAX_FILESIZE) {
        return -1;
    }
    
    int file_idx = -1;
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (filesystem.files[i].in_use && !str_compare(filesystem.files[i].name, path)) {
            file_idx = i;
            break;
        }
    }
    
    if (file_idx == -1) {
        if (fs_create(path, 0) < 0) {
            return -2;
        }
        for (u32 i = 0; i < FS_MAX_FILES; i++) {
            if (filesystem.files[i].in_use && !str_compare(filesystem.files[i].name, path)) {
                file_idx = i;
                break;
            }
        }
    }
    
    if (file_idx == -1) {
        return -3;
    }
    
    fs_file_t* file = &filesystem.files[file_idx];
    
    if (file->is_directory) {
        return -4;
    }
    
    u32 offset = file->offset;
    for (u32 i = 0; i < size; i++) {
        filesystem.data[offset + i] = data[i];
    }
    
    file->size = size;
    
    if (offset + size > next_data_offset) {
        next_data_offset = offset + size;
    }
    
    return size;
}

int fs_read(const char* path, u8* data, u32 max_size) {
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (filesystem.files[i].in_use && !str_compare(filesystem.files[i].name, path)) {
            fs_file_t* file = &filesystem.files[i];
            
            if (file->is_directory) {
                return -1;
            }
            
            u32 to_read = file->size < max_size ? file->size : max_size;
            
            for (u32 j = 0; j < to_read; j++) {
                data[j] = filesystem.data[file->offset + j];
            }
            
            return to_read;
        }
    }
    
    return -2;
}

int fs_delete(const char* path) {
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (filesystem.files[i].in_use && !str_compare(filesystem.files[i].name, path)) {
            filesystem.files[i].in_use = 0;
            filesystem.num_files--;
            return 0;
        }
    }
    
    return -1;
}

int fs_list(const char* path, char* buffer, u32 max_size) {
    u32 path_len = str_len(path);
    u32 pos = 0;
    
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (filesystem.files[i].in_use) {
            const char* name = filesystem.files[i].name;
            
            u8 match = 1;
            for (u32 j = 0; j < path_len; j++) {
                if (name[j] != path[j]) {
                    match = 0;
                    break;
                }
            }
            
            if (!match) continue;
            
            const char* short_name = name + path_len;
            if (*short_name == '/') short_name++;
            
            u8 in_subdir = 0;
            for (u32 j = 0; short_name[j]; j++) {
                if (short_name[j] == '/') {
                    in_subdir = 1;
                    break;
                }
            }
            
            if (!in_subdir && *short_name) {
                u32 name_len = str_len(short_name);
                if (pos + name_len + 2 > max_size) break;
                
                for (u32 j = 0; j < name_len; j++) {
                    buffer[pos++] = short_name[j];
                }
                
                if (filesystem.files[i].is_directory) {
                    buffer[pos++] = '/';
                }
                
                buffer[pos++] = '\n';
            }
        }
    }
    
    buffer[pos] = 0;
    return pos;
}

int fs_exists(const char* path) {
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (filesystem.files[i].in_use && !str_compare(filesystem.files[i].name, path)) {
            return 1;
        }
    }
    return 0;
}

u32 fs_size(const char* path) {
    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (filesystem.files[i].in_use && !str_compare(filesystem.files[i].name, path)) {
            return filesystem.files[i].size;
        }
    }
    return 0;
}
