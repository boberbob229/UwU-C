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

int fs_find(const char* path) {
    if (!path || !*path) {
        return -1;
    }

    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (filesystem.files[i].in_use && !str_compare(filesystem.files[i].name, path)) {
            return (int)i;
        }
    }
    return -1;
}


int fs_create(const char* path, u8 is_directory) {
    char fixed_path[256];
    str_copy(fixed_path, path);

    if (is_directory) {
        u32 len = str_len(fixed_path);
        if (fixed_path[len - 1] != '/') {
            fixed_path[len] = '/';
            fixed_path[len + 1] = 0;
        }
    }

    if (filesystem.num_files >= FS_MAX_FILES) {
        return -1;
    }

    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (filesystem.files[i].in_use &&
            !str_compare(filesystem.files[i].name, fixed_path)) {
            return -2;
            }
    }

    for (u32 i = 0; i < FS_MAX_FILES; i++) {
        if (!filesystem.files[i].in_use) {
            str_copy(filesystem.files[i].name, fixed_path);
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
        if (!filesystem.files[i].in_use)
            continue;

        const char* name = filesystem.files[i].name;

        u8 match = 1;
        for (u32 j = 0; j < path_len; j++) {
            if (name[j] != path[j]) {
                match = 0;
                break;
            }
        }
        if (!match)
            continue;

        if (str_len(name) == path_len)
            continue;

        const char* rel = name + path_len;

        if (path_len > 1 && *rel == '/')
            rel++;

        u32 slash_count = 0;
        u32 rel_len = str_len(rel);
        for (u32 j = 0; j < rel_len; j++) {
            if (rel[j] == '/')
                slash_count++;
        }

        if (slash_count > 1)
            continue;

        /* reject "file/thing" */
        if (slash_count == 1 && rel[rel_len - 1] != '/')
            continue;

        if (pos + rel_len + 2 >= max_size)
            break;

        for (u32 j = 0; j < rel_len; j++)
            buffer[pos++] = rel[j];

        buffer[pos++] = '\n';
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

int fs_is_directory(const char* path) {
    int idx = fs_find(path);
    if (idx < 0) return 0;
    return filesystem.files[idx].is_directory;
}

void fs_normalize_path(char* path) {
    u32 w = 0;
    for (u32 r = 0; path[r]; r++) {
        if (r > 0 && path[r] == '/' && path[r - 1] == '/') continue;
        path[w++] = path[r];
    }
    path[w] = 0;

    u32 len = str_len(path);
    if (len > 1 && path[len - 1] == '/') {
        path[len - 1] = 0;
    }
}
