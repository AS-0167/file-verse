#include "ofs_types.h"
#include "security.h"
#include "hash_table.h"
#include "fs_tree.h"
#include "bitmap.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>

typedef struct {
    uint64_t total_size; 
    uint32_t block_size; 
    uint32_t max_files; 
    uint32_t max_users;
    char admin_username[32]; 
    char admin_password[64]; 
    char private_key[64];
} OFSConfig;

static int load_config(const char* config_path, OFSConfig* config) {
    // Placeholder - implement actual config loading
    config->total_size = 10485760;  // 10MB default
    config->block_size = 4096;
    config->max_files = 1024;
    config->max_users = 64;
    return 0;
}

int fs_format(const char* omni_path, const char* config_path) {
    OFSConfig config; 
    load_config(config_path, &config);
    FILE* f = fopen(omni_path, "wb");
    if (!f) return OFS_ERROR_IO;
    
    OMNIHeader header = {0};
    strcpy(header.magic, "OMNIFS01");
    header.total_size = config.total_size; 
    header.max_files = config.max_files;
    header.max_users = config.max_users; 
    header.block_size = config.block_size;
    
    header.user_table_offset = sizeof(OMNIHeader);
    header.user_table_size = config.max_users * sizeof(UserInfo);
    header.metadata_offset = header.user_table_offset + header.user_table_size;
    header.metadata_size = config.max_files * sizeof(FileEntry);
    header.freespace_offset = header.metadata_offset + header.metadata_size;
    
    uint64_t remaining_space = header.total_size - header.freespace_offset;
    header.total_blocks = remaining_space / (header.block_size + 1);
    header.freespace_size = (header.total_blocks + 7) / 8;
    header.content_offset = header.freespace_offset + header.freespace_size;
    
    fwrite(&header, sizeof(OMNIHeader), 1, f);
    
    UserInfo admin = {0};
    strncpy(admin.username, "admin", 31);
    hash_password("admin123", admin.password_hash);
    admin.role = ROLE_ADMIN; 
    admin.user_id = 1; 
    admin.is_active = 1;
    fwrite(&admin, sizeof(UserInfo), 1, f);
    
    UserInfo empty_user = {0};
    for (uint32_t i = 1; i < header.max_users; i++) {
        fwrite(&empty_user, sizeof(UserInfo), 1, f);
    }
    
    fseek(f, header.metadata_offset, SEEK_SET);
    
    // Index 0: Invalid/unused entry
    FileEntry empty_entry = {0}; 
    empty_entry.is_valid = 1;  // marked as invalid
    fwrite(&empty_entry, sizeof(FileEntry), 1, f);
    
    // Index 1: Root directory
    FileEntry root = {0};
    root.is_valid = 0;  // valid entry
    root.is_directory = 1; 
    root.entry_index = 1;
    strcpy(root.name, "/"); 
    root.owner_id = 1;
    root.parent_index = 1;  // root's parent is itself
    root.created_time = time(NULL);
    root.modified_time = root.created_time;
    fwrite(&root, sizeof(FileEntry), 1, f);
    
    // Fill remaining entries as invalid
    for(uint32_t i = 2; i < header.max_files; i++) {
        fwrite(&empty_entry, sizeof(FileEntry), 1, f);
    }
    
    long current_pos = ftell(f);
    if(current_pos < header.total_size) {
        ftruncate(fileno(f), header.total_size);
    }

    fclose(f);
    printf("File system created successfully!\n");
    return OFS_SUCCESS;
}

int fs_init(void** instance, const char* omni_path, const char* config_path) {
    OFSInstance* ofs = (OFSInstance*)calloc(1, sizeof(OFSInstance));
    if (!ofs) return OFS_ERROR_SYSTEM;
    
    ofs->omni_file = fopen(omni_path, "r+b");
    if (!ofs->omni_file) {
        free(ofs);
        return OFS_ERROR_IO;
    }
    
    if (fread(&ofs->header, sizeof(OMNIHeader), 1, ofs->omni_file) != 1) {
        fclose(ofs->omni_file); free(ofs); return OFS_ERROR_IO;
    }

    if (strncmp(ofs->header.magic, "OMNIFS01", 8) != 0) {
        fclose(ofs->omni_file); free(ofs); return OFS_ERROR_CORRUPTED;
    }

    // Load users
    ofs->users = ht_create(ofs->header.max_users);
    fseek(ofs->omni_file, ofs->header.user_table_offset, SEEK_SET);
    for (uint32_t i = 0; i < ofs->header.max_users; i++) {
        UserInfo* user = (UserInfo*)malloc(sizeof(UserInfo));
        if(!user || fread(user, sizeof(UserInfo), 1, ofs->omni_file) != 1) {
             if(user) free(user); continue; 
        }
        if (user->is_active) {
            ht_insert(ofs->users, user->username, user);
        } else {
            free(user);
        }
    }

    printf("INIT: Rebuilding file system tree from disk...\n");
    ofs->file_tree = fstree_create();
    
    FileEntry* entries = (FileEntry*)malloc(ofs->header.metadata_size);
    fseek(ofs->omni_file, ofs->header.metadata_offset, SEEK_SET);
    fread(entries, sizeof(FileEntry), ofs->header.max_files, ofs->omni_file);

    FSNode** node_map = (FSNode**)calloc(ofs->header.max_files, sizeof(FSNode*));
    node_map[1] = ofs->file_tree->root;
    
    uint32_t max_used_index = 1;
    for (uint32_t i = 2; i < ofs->header.max_files; i++) {
        if (entries[i].is_valid == 0) {
            if (entries[i].entry_index > max_used_index) {
                max_used_index = entries[i].entry_index;
            }
        }
    }
    ofs->next_entry_index = max_used_index + 1;
    printf("INIT: Set next_entry_index to %u\n", ofs->next_entry_index);

    // First pass: Create all nodes
    for (uint32_t i = 2; i < ofs->header.max_files; i++) {
        if (entries[i].is_valid == 0) {
            FSNode* new_node = fsnode_create(entries[i].name, entries[i].is_directory);
            if (!new_node) continue;
            new_node->entry_index = entries[i].entry_index;
            // ... copy other metadata ...
            node_map[i] = new_node;
        }
    }
    
    // Second pass: Link nodes and build paths
    for (uint32_t i = 2; i < ofs->header.max_files; i++) {
        if (node_map[i] != NULL) {
            uint32_t parent_idx = entries[i].parent_index;
            if (parent_idx >= ofs->header.max_files || !node_map[parent_idx]) {
                printf("WARNING: Entry %u has invalid parent. Skipping.\n", i);
                fsnode_destroy(node_map[i]); // Corrected from fsnode_free
                node_map[i] = NULL;
                continue;
            }
            
            FSNode* parent_node = node_map[parent_idx];
            
            // --- SIMPLIFIED AND CORRECTED PATH BUILDING ---
            char parent_full_path[MAX_PATH_LENGTH];
            // Get the parent's full path from the path_cache (it must already be there)
            // We need to iterate the hash table to find the key for a given value.
            // This is inefficient. A better approach is to build paths as we go.
            // For now, let's use a simpler method.
            char* temp_parent_path = fstree_get_path(parent_node); // Assuming fstree_get_path is implemented
            if (temp_parent_path) {
                strncpy(parent_full_path, temp_parent_path, MAX_PATH_LENGTH - 1);
                free(temp_parent_path);
            } else {
                strcpy(parent_full_path, "/"); // Fallback
            }
            
            char child_path[MAX_PATH_LENGTH];
            if(strcmp(parent_full_path, "/") == 0) {
                snprintf(child_path, sizeof(child_path), "/%s", node_map[i]->name);
            } else {
                snprintf(child_path, sizeof(child_path), "%s/%s", parent_full_path, node_map[i]->name);
            }
            // --- END OF SIMPLIFIED LOGIC ---
            
            if (fstree_add_node(ofs->file_tree, child_path, node_map[i]) != 0) {
                printf("WARNING: Failed to add node at path '%s'\n", child_path);
            }
        }
    }
    
    free(node_map);
    free(entries);
    printf("INIT: Tree rebuilt successfully.\n");
    
    // Load bitmap
    fseek(ofs->omni_file, ofs->header.freespace_offset, SEEK_SET);
    uint8_t* bitmap_data = (uint8_t*)malloc(ofs->header.freespace_size);
    fread(bitmap_data, ofs->header.freespace_size, 1, ofs->omni_file);
    ofs->free_blocks = bitmap_load(bitmap_data, ofs->header.total_blocks);
    free(bitmap_data);
    
    ofs->sessions = ht_create(128);
    *instance = ofs;
    
    return OFS_SUCCESS;
}


// Helper function to free UserInfo values
static void free_user_info(void* value) {
    if (value) free(value);
}

// Helper function to free SessionInfo values
static void free_session_info(void* value) {
    if (value) free(value);
}


void fs_shutdown(void* instance) {
    if (!instance) return;
    
    OFSInstance* ofs = (OFSInstance*)instance;
    
    // Helper function to free the allocated value pointers in a hash table
    void free_value(void* value) {
        if (value) free(value);
    }
    
    if (ofs->omni_file) fclose(ofs->omni_file);
    if (ofs->users) ht_destroy(ofs->users, free_value);
    if (ofs->file_tree) fstree_destroy(ofs->file_tree);
    if (ofs->free_blocks) bitmap_destroy(ofs->free_blocks);
    if (ofs->sessions) ht_destroy(ofs->sessions, free_value);
    
    free(ofs);
}
