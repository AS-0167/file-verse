#ifndef OFS_TYPES_H
#define OFS_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>

// FORWARD DECLARATION to break the include cycle.
// OFSystem only needs to know that UserMap is a type it can have a pointer to.
// The full details of UserMap are not needed here.
struct UserMap;

// --- On-Disk Data Structures ---
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

// --- Helper & In-Memory Structures ---
struct DirEntryInfo {
    std::string name;
    bool is_directory;
};

struct FSStats {
    uint64_t total_size;
    uint64_t used_space;
    uint64_t free_space;
    uint32_t file_count;
    uint32_t directory_count;
};

struct FileMetadata {
    std::string name;
    bool is_directory;
    uint64_t size;
    uint32_t owner_id;
    uint32_t permissions;
    uint64_t created_time;
    uint64_t modified_time;
};

struct SessionInfo {
    std::string username;
    uint32_t role;
};

struct OFSystem {
    OMNIHeader header;
    std::vector<UserInfo> user_table;
    UserMap* user_map; // This pointer is now valid because of the forward declaration
    std::map<std::string, UserInfo*> active_sessions;
    std::vector<MetadataEntry> metadata_entries;
    std::vector<bool> free_block_map;
    std::string omni_filepath;
};

#endif // OFS_TYPES_H