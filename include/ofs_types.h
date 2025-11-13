#ifndef OFS_TYPES_H
#define OFS_TYPES_H

#include "hash_table.h" // Your hash table header
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// OFFICIAL PROJECT DATA STRUCTURES - DO NOT MODIFY
// These are now aligned with the project specification for compatibility.
// ============================================================================

/**
 * OMNI File Header (512 bytes total)
 * Located at the beginning of every .omni file
 */
struct OMNIHeader {
    char magic[8];              // Magic number: "OMNIFS01"
    uint32_t format_version;    // Format version: 0x00010000 for v1.0
    uint64_t total_size;        // Total file system size in bytes
    uint64_t header_size;       // Size of this header (must be 512)
    uint64_t block_size;        // Block size in bytes (e.g., 4096)
    char student_id[32];        // Your student ID
    char submission_date[16];   // Creation date YYYY-MM-DD
    char config_hash[64];       // SHA-256 hash of config file
    uint64_t config_timestamp;  // Config file timestamp
    uint32_t user_table_offset; // Byte offset to user table
    uint32_t max_users;         // Maximum number of users
    uint32_t file_state_storage_offset; // Reserved for Phase 2
    uint32_t change_log_offset;       // Reserved for Phase 2
    uint8_t reserved[328];      // Reserved for future use
};

/**
 * User Information Structure (128 bytes total)
 * Stored in user table within .omni file
 */
struct UserInfo {
    char username[32];          // Username (null-terminated)
    char password_hash[64];     // Password hash (SHA-256)
    uint32_t role;              // 0 = NORMAL, 1 = ADMIN (from UserRole enum)
    uint64_t created_time;      // Account creation timestamp (Unix epoch)
    uint64_t last_login;        // Last login timestamp (Unix epoch)
    uint8_t is_active;          // 1 if active, 0 if deleted/free
    uint8_t reserved[23];       // Reserved for future use
};

/**
 * File/Directory Metadata Structure (72 bytes total)
 * This is the structure for the Metadata Index Area.
 */
struct MetadataEntry {
    uint8_t  validity_flag;   // 0 = In Use, 1 = Unused/Free
    uint8_t  type_flag;       // 0 = File, 1 = Directory
    uint32_t parent_index;    // Entry Index of parent. Root is 0.
    char     short_name[12];  // Up to 10 chars + null terminator.
    uint32_t start_index;     // Block Index where content begins. 0 if empty.
    uint64_t total_size;      // Logical size of the file content in bytes.
    uint32_t owner_id;        // Index into the User Table for the owner.
    uint32_t permissions;     // UNIX-style permissions (e.g., 0644)
    uint64_t created_time;
    uint64_t modified_time;
    uint8_t  reserved[14];    // Padding to make the struct exactly 72 bytes
};

// ============================================================================
// IN-MEMORY "BRAIN" OF THE FILE SYSTEM
// This struct holds the live, loaded state of the filesystem.
// It does NOT get saved directly to disk.
// ============================================================================
struct FileSystemInstance {
    OMNIHeader header;
    std::vector<UserInfo> user_table;       // Master list of all users
    HashTable* user_hash_table;             // Fast lookup table (maps username -> &user_table[i])
    std::vector<MetadataEntry> metadata_entries;
    std::vector<bool> free_block_map;
    std::string omni_file_path;
};

#endif // OFS_TYPES_H