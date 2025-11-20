#ifndef OFS_TYPES_H
#define OFS_TYPES_H

#include "hash_table.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

// ============================================================================
// OFFICIAL PROJECT DATA STRUCTURES
// ============================================================================

struct OMNIHeader {
    char magic[8];
    uint32_t format_version;
    uint64_t total_size;
    uint64_t header_size;
    uint64_t block_size;
    char student_id[32];
    char submission_date[16];
    char config_hash[64];
    uint64_t config_timestamp;
    uint32_t user_table_offset;
    uint32_t max_users;
    uint32_t file_state_storage_offset;
    uint32_t change_log_offset;
    uint8_t reserved[328];
};

struct UserInfo {
    char username[32];
    char password_hash[64];
    uint32_t role;
    uint64_t created_time;
    uint64_t last_login;
    uint8_t is_active;
    uint8_t reserved[23];
};

struct MetadataEntry {
    uint8_t  validity_flag;
    uint8_t  type_flag;
    uint32_t parent_index;
    char     short_name[12];
    uint32_t start_index;
    uint64_t total_size;
    uint32_t owner_id;
    uint32_t permissions;
    uint64_t created_time;
    uint64_t modified_time;
    uint8_t  reserved[14];
};

// ============================================================================
// HELPER & UTILITY STRUCTURES
// ============================================================================

// Struct for returning directory listing information
struct DirEntryInfo {
    std::string name;
    bool is_directory;
};

// Struct for returning file system statistics
struct FSStats {
    uint64_t total_size;
    uint64_t used_space;
    uint64_t free_space;
    uint32_t file_count;
    uint32_t directory_count;
};

// Struct for returning detailed file/directory metadata
struct FileMetadata {
    std::string name;
    bool is_directory;
    uint64_t size;
    uint32_t owner_id;
    uint32_t permissions;
    uint64_t created_time;
    uint64_t modified_time;
};

// Struct for returning session information
struct SessionInfo {
    std::string username;
    uint32_t role;
};

// ============================================================================
// IN-MEMORY "BRAIN" OF THE FILE SYSTEM
// ============================================================================
struct FileSystemInstance {
    OMNIHeader header;
    std::vector<UserInfo> user_table;
    HashTable* user_hash_table;
    std::map<std::string, UserInfo*> active_sessions;
    std::vector<MetadataEntry> metadata_entries;
    std::vector<bool> free_block_map;
    std::string omni_file_path;
};

#endif // OFS_TYPES_H