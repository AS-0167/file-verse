// In file: source/include/ofs_types.h

#ifndef OFS_TYPES_H
#define OFS_TYPES_H

#include "hash_table.h"
#include <cstdint>
#include <string>
#include <vector>

// On-disk file header
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

// On-disk user information
struct UserInfo {
    char username[32];
    char password_hash[64];
    uint32_t role;
    uint64_t created_time;
    uint64_t last_login;
    uint8_t is_active;
    uint8_t reserved[23];
};

// On-disk file/directory metadata
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

// The main IN-MEMORY struct that holds the entire live state of the file system.
struct FileSystemInstance {
    OMNIHeader header;
    std::vector<UserInfo> user_table;
    HashTable* user_hash_table;
    std::vector<MetadataEntry> metadata_entries;
    std::vector<bool> free_block_map;
    std::string omni_file_path;
};

#endif // OFS_TYPES_H