#include "ofs_types.h"
#include "fs_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitmap.h"
#include <time.h>

// ============================================================================
// dir_create (CLEANED VERSION)
// ============================================================================
int dir_create(OFSInstance* ofs, SessionInfo* session, const char* path) {
    // Validate inputs
    if (!ofs || !session || !path) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // Check if we have space for a new metadata entry
    if (ofs->next_entry_index >= ofs->header.max_files) {
        return OFS_ERROR_NO_SPACE;
    }

    // Check if path already exists
    if (fstree_find(ofs->file_tree, path) != NULL) {
        return OFS_ERROR_ALREADY_EXISTS;
    }

    // --- Extract parent path and new directory name ---
    char parent_path[MAX_PATH_LENGTH];
    strncpy(parent_path, path, sizeof(parent_path) - 1);
    parent_path[sizeof(parent_path) - 1] = '\0';

    char* last_slash = strrchr(parent_path, '/');
    if (!last_slash) {
        return OFS_ERROR_INVALID_PARAM;
    }

    const char* name = last_slash + 1;

    // Validate name length
    if (strlen(name) == 0 || strlen(name) > 11) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // Calculate parent path string
    if (last_slash == parent_path) {
        strcpy(parent_path, "/");
    } else {
        *last_slash = '\0';
    }

    // --- Find and validate parent node ---
    FSNode* parent_node = fstree_find(ofs->file_tree, parent_path);
    if (!parent_node) {
        return OFS_ERROR_NOT_FOUND;
    }
    if (!parent_node->is_directory) {
        return OFS_ERROR_NOT_DIRECTORY;
    }

    // --- Create and write the new FileEntry to disk ---
    uint32_t new_index = ofs->next_entry_index;

    FileEntry new_entry;
    memset(&new_entry, 0, sizeof(FileEntry));
    new_entry.is_valid = 0; // 0 means valid
    new_entry.is_directory = 1;
    new_entry.parent_index = parent_node->entry_index;
    new_entry.entry_index = new_index;
    new_entry.owner_id = session->user_id;
    new_entry.created_time = time(NULL);
    new_entry.modified_time = new_entry.created_time;
    new_entry.permissions = 0755; // Default directory permissions

    strncpy(new_entry.name, name, 11);
    new_entry.name[11] = '\0';

    uint64_t offset = ofs->header.metadata_offset + (new_index * sizeof(FileEntry));
    if (offset + sizeof(FileEntry) > ofs->header.metadata_offset + ofs->header.metadata_size) {
        return OFS_ERROR_SYSTEM; // Should not happen if space check is correct
    }

    if (fseek(ofs->omni_file, offset, SEEK_SET) != 0) {
        return OFS_ERROR_IO;
    }
    if (fwrite(&new_entry, sizeof(FileEntry), 1, ofs->omni_file) != 1) {
        return OFS_ERROR_IO;
    }
    fflush(ofs->omni_file);

    // --- Create the in-memory FSNode and add it to the tree ---
    FSNode* new_node = fsnode_create(new_entry.name, 1);
    if (!new_node) {
        // In a more robust system, you would mark the on-disk entry as invalid here
        return OFS_ERROR_SYSTEM;
    }

    // Copy relevant metadata from the on-disk entry to the in-memory node
    new_node->entry_index = new_entry.entry_index;
    new_node->owner_id = new_entry.owner_id;
    new_node->created_time = new_entry.created_time;
    new_node->modified_time = new_entry.modified_time;
    new_node->permissions = new_entry.permissions;
    new_node->size = 0;
    new_node->start_block = 0;

    if (fstree_add_node(ofs->file_tree, path, new_node) != 0) {
        free(new_node);
        return OFS_ERROR_SYSTEM;
    }

    // Only increment the global index after all steps have succeeded
    ofs->next_entry_index++;

    return OFS_SUCCESS;
}

// ============================================================================
// dir_list
// ============================================================================
int dir_list(OFSInstance* ofs, SessionInfo* session, const char* path,
             FileEntry** entries_out, int* count_out) {
    if (!ofs || !session || !path || !entries_out || !count_out) {
        return OFS_ERROR_INVALID_PARAM;
    }

    FSNode* dir_node = fstree_find(ofs->file_tree, path);
    if (!dir_node) {
        return OFS_ERROR_NOT_FOUND;
    }
    if (!dir_node->is_directory) {
        return OFS_ERROR_NOT_DIRECTORY;
    }

    size_t child_node_count;
    FSNode** child_nodes = fstree_list_children(dir_node, &child_node_count);
    *count_out = (int)child_node_count;

    if (child_node_count == 0) {
        *entries_out = NULL;
        if(child_nodes) free(child_nodes);
        return OFS_SUCCESS;
    }

    *entries_out = (FileEntry*)malloc(child_node_count * sizeof(FileEntry));
    if (!(*entries_out)) {
        free(child_nodes);
        return OFS_ERROR_SYSTEM;
    }

    for (size_t i = 0; i < child_node_count; i++) {
        FSNode* child = child_nodes[i];
        FileEntry* entry = &((*entries_out)[i]);

        entry->is_valid = 0;
        entry->is_directory = child->is_directory;
        entry->parent_index = dir_node->entry_index;
        strncpy(entry->name, child->name, 11);
        entry->name[11] = '\0';
        entry->start_block = child->start_block;
        entry->total_size = child->size;
        entry->owner_id = child->owner_id;
        entry->permissions = child->permissions;
        entry->created_time = child->created_time;
        entry->modified_time = child->modified_time;
        entry->entry_index = child->entry_index;
    }

    free(child_nodes);
    return OFS_SUCCESS;
}


// ============================================================================
// dir_delete
// ============================================================================
int dir_delete(OFSInstance* ofs, SessionInfo* session, const char* path) {
    if (!ofs || !session || !path) {
        return OFS_ERROR_INVALID_PARAM;
    }

    FSNode* node = fstree_find(ofs->file_tree, path);
    if (!node || !node->is_directory) {
        return OFS_ERROR_NOT_FOUND;
    }

    if (node->children->size > 0) {
        return OFS_ERROR_NOT_EMPTY;
    }

    // Mark entry as invalid on disk
    FileEntry entry;
    memset(&entry, 0, sizeof(FileEntry));
    entry.is_valid = 1;  // 1 means invalid

    uint64_t offset = ofs->header.metadata_offset +
                      (node->entry_index * sizeof(FileEntry));

    fseek(ofs->omni_file, offset, SEEK_SET);
    fwrite(&entry, sizeof(FileEntry), 1, ofs->omni_file);
    fflush(ofs->omni_file);

    // Remove from in-memory tree
    fstree_remove(ofs->file_tree, path);

    return OFS_SUCCESS;
}


// ============================================================================
// get_stats - Retrieves statistics about the entire file system
// ============================================================================
int get_stats(OFSInstance* ofs, SessionInfo* session, FSStats* stats_out) {
    // 1. --- VALIDATE INPUTS ---
    if (!ofs || !session || !stats_out) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 2. --- POPULATE STATS FROM HEADER AND BITMAP ---
    // These stats are easy to get directly from the header and the free block bitmap.
    stats_out->total_size = ofs->header.total_size;
    stats_out->total_blocks = ofs->header.total_blocks;
    stats_out->free_blocks = bitmap_count_free(ofs->free_blocks); // You'll need to implement bitmap_count_free
    stats_out->used_blocks = stats_out->total_blocks - stats_out->free_blocks;
    stats_out->free_space = (uint64_t)stats_out->free_blocks * ofs->header.block_size;

    // 3. --- CALCULATE USED SPACE AND TOTAL FILES ---
    // To get these, we must iterate through all the file/directory entries.
    // The most reliable way is to traverse the in-memory tree.
    stats_out->used_space = 0;
    // The size of the path cache hash table gives us the total number of files and directories.
    stats_out->total_files = ofs->file_tree->path_cache->size;

    // To calculate used space, we need to iterate through all file nodes.
    // A simple way is to iterate through the path cache hash table.
    for (size_t i = 0; i < ofs->file_tree->path_cache->capacity; i++) {
        HashNode* node = ofs->file_tree->path_cache->buckets[i];
        while (node) {
            FSNode* fs_node = (FSNode*)node->value;
            // We only add the size of files, not directories
            if (fs_node && !fs_node->is_directory) {
                stats_out->used_space += fs_node->size;
            }
            node = node->next;
        }
    }

    return OFS_SUCCESS;
}

// ============================================================================
// dir_exists - Checks if a directory exists at the given path
// ============================================================================
int dir_exists(OFSInstance* ofs, SessionInfo* session, const char* path) {
    // 1. Validate inputs (session is required by the API but not used in the logic)
    if (!ofs || !session || !path) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 2. Find the node using the in-memory file system tree
    FSNode* node = fstree_find(ofs->file_tree, path);

    // 3. Check if the node was found AND that it is a directory
    if (node && node->is_directory) {
        return OFS_SUCCESS; // Success means "yes, a directory exists here"
    }

    // If the node was not found, or if it was a file, the check fails.
    return OFS_ERROR_NOT_FOUND;
}