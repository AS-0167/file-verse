#ifndef OFS_TYPES_H
#define OFS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>

// --- FORWARD DECLARATIONS ---
typedef struct HashTable HashTable;
typedef struct FSNode FSNode;
typedef struct FSTree FSTree;
typedef struct Bitmap Bitmap;

// --- CONSTANTS & BASIC ENUMS ---
#define MAX_PATH_LENGTH 256
typedef enum {
    OFS_SUCCESS = 0,
    OFS_ERROR_INVALID_PARAM = -1,
    OFS_ERROR_PERMISSION_DENIED = -2,
    OFS_ERROR_NOT_FOUND = -3,
    OFS_ERROR_ALREADY_EXISTS = -4,
    OFS_ERROR_NOT_EMPTY = -5,
    OFS_ERROR_NO_SPACE = -6,
    OFS_ERROR_IO = -7,
    OFS_ERROR_CORRUPTED = -8,
    OFS_ERROR_AUTH_FAILED = -9,
    OFS_ERROR_SESSION_INVALID = -10,
    OFS_ERROR_NOT_DIRECTORY = -11,
    OFS_ERROR_IS_DIRECTORY = -12,
    OFS_ERROR_SYSTEM = -13,
    OFS_ERROR_NOT_A_FILE = -14,
    OFS_ERROR_FILE_TOO_LARGE = -15
} OFSErrorCodes;
typedef enum { ROLE_ADMIN = 0, ROLE_USER = 1, ROLE_GUEST = 2 } UserRole;

// --- ON-DISK STRUCTS ---
typedef struct {
    char username[32];
    char password_hash[64];
    UserRole role;
    uint32_t user_id;
    uint8_t is_active;
    char reserved[83];
} UserInfo;

typedef struct {
    uint8_t is_valid;
    uint8_t is_directory;
    uint32_t parent_index;
    char name[12]; // This MUST be 12
    uint32_t start_block;
    uint64_t total_size;
    uint32_t owner_id;
    uint32_t permissions;
    uint64_t created_time;
    uint64_t modified_time;
    uint32_t entry_index;
    char reserved[16];
} FileEntry;

typedef struct {
    char magic[8];
    uint32_t version;
    uint64_t total_size;
    uint32_t block_size;
    uint32_t max_files;
    uint32_t max_users;
    uint32_t user_table_offset;
    uint32_t user_table_size;
    uint32_t metadata_offset;
    uint32_t metadata_size;
    uint32_t freespace_offset;
    uint32_t freespace_size;
    uint32_t content_offset;
    uint32_t content_size;
    uint32_t total_blocks;
    char reserved[256];
} OMNIHeader;

// --- IN-MEMORY STRUCTS ---
typedef struct {
    char session_id[64];
    uint32_t user_id;
    char username[32];
    UserRole role;
    uint64_t login_time;
    uint8_t is_valid;
    char reserved[47];
} SessionInfo;

typedef struct {
    FILE* omni_file;
    OMNIHeader header;
    HashTable* users;
    FSTree* file_tree;
    Bitmap* free_blocks;
    HashTable* sessions;
    char omni_path[256];
    uint32_t next_user_id;
    uint32_t next_entry_index;
} OFSInstance;

typedef struct {
    uint64_t total_size;
    uint64_t used_space;
    uint64_t free_space;
    uint32_t total_blocks;
    uint32_t used_blocks;
    uint32_t free_blocks;
    uint32_t total_files;
} FSStats;

#endif // OFS_TYPES_H