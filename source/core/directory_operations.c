#include "ofs_types.h"
#include "fs_tree.h"
#include "bitmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <time.h>

int dir_create(OFSInstance* ofs, SessionInfo* session, const char* path) {
    if (!ofs || !session || !path) {
        printf("[ERROR] dir_create: Invalid parameters.\n");
        return OFS_ERROR_INVALID_PARAM;
    }

    // Make a copy of the path to safely modify it
    char path_copy[MAX_PATH_LENGTH];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    // Remove trailing slash unless root
    size_t len = strlen(path_copy);
    if (len > 1 && path_copy[len - 1] == '/') {
        path_copy[len - 1] = '\0';
    }

    // Extract name and parent path
    char* last_slash = strrchr(path_copy, '/');
    if (!last_slash) {
        printf("[ERROR] dir_create: Invalid path format (no slash).\n");
        return OFS_ERROR_INVALID_PARAM;
    }

    const char* name = last_slash + 1;
    if (strlen(name) == 0) {
        printf("[ERROR] dir_create: Directory name is empty!\n");
        return OFS_ERROR_INVALID_PARAM;
    }

    char parent_path[MAX_PATH_LENGTH];
    if (last_slash == path_copy) {
        // Parent is root
        strcpy(parent_path, "/");
    } else {
        *last_slash = '\0';
        strncpy(parent_path, path_copy, sizeof(parent_path) - 1);
        parent_path[sizeof(parent_path) - 1] = '\0';
    }

    // Check metadata space
    if (ofs->next_entry_index >= ofs->header.max_files) {
        printf("[ERROR] dir_create: No space for new metadata entry.\n");
        return OFS_ERROR_NO_SPACE;
    }

    // Check if path already exists
    if (fstree_find(ofs->file_tree, path) != NULL) {
        printf("[ERROR] dir_create: Path '%s' already exists.\n", path);
        return OFS_ERROR_ALREADY_EXISTS;
    }

    // Find parent node
    FSNode* parent_node = fstree_find(ofs->file_tree, parent_path);
    if (!parent_node) {
        printf("[ERROR] dir_create: Parent directory '%s' not found.\n", parent_path);
        return OFS_ERROR_NOT_FOUND;
    }
    if (!parent_node->is_directory) {
        printf("[ERROR] dir_create: Parent path '%s' is not a directory.\n", parent_path);
        return OFS_ERROR_NOT_DIRECTORY;
    }

    // Prepare new FileEntry
    FileEntry new_entry;
    memset(&new_entry, 0, sizeof(FileEntry));

    uint32_t new_index = ofs->next_entry_index;
    new_entry.is_valid = 0;
    new_entry.is_directory = 1;
    new_entry.parent_index = parent_node->entry_index;
    new_entry.entry_index = new_index;
    new_entry.owner_id = session->user_id;
    new_entry.created_time = time(NULL);
    new_entry.modified_time = new_entry.created_time;
    new_entry.permissions = 0755;
    snprintf(new_entry.name, sizeof(new_entry.name), "%s", name);

    // Write to disk safely
    uint64_t offset = ofs->header.metadata_offset + (new_index * sizeof(FileEntry));
    if (fseek(ofs->omni_file, offset, SEEK_SET) != 0) {
        printf("[ERROR] dir_create: fseek failed (errno=%d)\n", errno);
        return OFS_ERROR_IO;
    }
    if (fwrite(&new_entry, sizeof(FileEntry), 1, ofs->omni_file) != 1) {
        printf("[ERROR] dir_create: fwrite failed\n");
        return OFS_ERROR_IO;
    }
    fflush(ofs->omni_file);
    printf("[DEBUG] FileEntry successfully written to disk at offset %llu.\n", (unsigned long long)offset);

    // Create in-memory node
    FSNode* new_node = fsnode_create(new_entry.name, 1);
    if (!new_node) {
        printf("[ERROR] dir_create: Failed to allocate memory for FSNode.\n");
        return OFS_ERROR_SYSTEM;
    }
    new_node->entry_index = new_index;
    new_node->owner_id = session->user_id;
    new_node->created_time = new_entry.created_time;
    new_node->modified_time = new_entry.modified_time;
    new_node->permissions = new_entry.permissions;

    // Add to in-memory tree
    if (fstree_add_node(ofs->file_tree, path, new_node) != 0) {
        free(new_node);
        printf("[ERROR] dir_create: Failed to add node to tree.\n");
        return OFS_ERROR_SYSTEM;
    }

    ofs->next_entry_index++;
    printf("[DEBUG] dir_create: Directory '%s' created successfully. next_entry_index=%u\n", name, ofs->next_entry_index);
    return OFS_SUCCESS;
}

// ... All other functions (dir_list, etc.) are included below ...
int dir_list(OFSInstance* ofs, SessionInfo* session, const char* path, FileEntry** entries_out, int* count_out) {
    printf("\n========== [DEBUG] dir_list: ENTER FUNCTION ==========\n");

    if (!ofs || !session || !path || !entries_out || !count_out) {
        printf("[ERROR] dir_list: Invalid parameters. ofs=%p, session=%p, path=%p, entries_out=%p, count_out=%p\n",
               (void*)ofs, (void*)session, (void*)path, (void*)entries_out, (void*)count_out);
        return OFS_ERROR_INVALID_PARAM;
    }
    printf("[DEBUG] dir_list: Listing directory for path '%s'\n", path);

    FSNode* dir_node = fstree_find(ofs->file_tree, path);
    if (!dir_node) {
        printf("[ERROR] dir_list: Directory node not found for path '%s'\n", path);
        return OFS_ERROR_NOT_FOUND;
    }
    if (!dir_node->is_directory) {
        printf("[ERROR] dir_list: Path '%s' exists but is not a directory\n", path);
        return OFS_ERROR_NOT_FOUND;
    }

    printf("[DEBUG] dir_list: Found directory node. entry_index=%u, name='%s'\n", dir_node->entry_index, dir_node->name);

    size_t child_node_count;
    FSNode** child_nodes = fstree_list_children(dir_node, &child_node_count);
    if (!child_nodes && child_node_count > 0) {
        printf("[ERROR] dir_list: fstree_list_children returned NULL but child_node_count=%zu\n", child_node_count);
        return OFS_ERROR_SYSTEM;
    }

    printf("[DEBUG] dir_list: Found %zu children\n", child_node_count);
    *count_out = (int)child_node_count;

    if (child_node_count == 0) {
        *entries_out = NULL;
        if (child_nodes) free(child_nodes);
        printf("[DEBUG] dir_list: Directory is empty.\n");
        return OFS_SUCCESS;
    }

    *entries_out = (FileEntry*)malloc(child_node_count * sizeof(FileEntry));
    if (!(*entries_out)) {
        printf("[ERROR] dir_list: Failed to allocate memory for entries_out\n");
        free(child_nodes);
        return OFS_ERROR_SYSTEM;
    }

    for (size_t i = 0; i < child_node_count; i++) {
        FSNode* child = child_nodes[i];
        FileEntry* entry = &((*entries_out)[i]);

        entry->is_valid = 0;  // 0 = valid
        entry->is_directory = child->is_directory;
        entry->parent_index = dir_node->entry_index;
        strncpy(entry->name, child->name, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';
        entry->total_size = child->size;
        entry->entry_index = child->entry_index;
        entry->owner_id = child->owner_id;
        entry->permissions = child->permissions;
        entry->created_time = child->created_time;
        entry->modified_time = child->modified_time;

        printf("[DEBUG] dir_list: Child[%zu] = name='%s', entry_index=%u, is_directory=%d, size=%llu\n",
               i, entry->name, entry->entry_index, entry->is_directory, (unsigned long long)entry->total_size);
    }

    free(child_nodes);
    printf("========== [DEBUG] dir_list: EXIT FUNCTION SUCCESS ==========\n");
    return OFS_SUCCESS;
}


int dir_delete(OFSInstance* ofs, SessionInfo* session, const char* path) {
    if (!ofs || !session || !path) return OFS_ERROR_INVALID_PARAM;
    FSNode* node = fstree_find(ofs->file_tree, path);
    if (!node || !node->is_directory) return OFS_ERROR_NOT_FOUND;
    if (node->children->size > 0) return OFS_ERROR_NOT_EMPTY;
    FileEntry entry;
    memset(&entry, 0, sizeof(FileEntry));
    entry.is_valid = 1;
    uint64_t offset = ofs->header.metadata_offset + (node->entry_index * sizeof(FileEntry));
    fseek(ofs->omni_file, offset, SEEK_SET);
    fwrite(&entry, sizeof(FileEntry), 1, ofs->omni_file);
    fflush(ofs->omni_file);
    fstree_remove(ofs->file_tree, path);
    fsnode_destroy(node);
    return OFS_SUCCESS;
}

int get_stats(OFSInstance* ofs, SessionInfo* session, FSStats* stats_out) {
    if (!ofs || !session || !stats_out) return OFS_ERROR_INVALID_PARAM;
    stats_out->total_size = ofs->header.total_size;
    stats_out->total_blocks = ofs->header.total_blocks;
    stats_out->free_blocks = bitmap_count_free(ofs->free_blocks);
    stats_out->used_blocks = stats_out->total_blocks - stats_out->free_blocks;
    stats_out->free_space = (uint64_t)stats_out->free_blocks * ofs->header.block_size;
    stats_out->used_space = 0;
    stats_out->total_files = ofs->file_tree->path_cache->size;
    for (size_t i = 0; i < ofs->file_tree->path_cache->capacity; i++) {
        HashNode* node = ofs->file_tree->path_cache->buckets[i];
        while (node) {
            FSNode* fs_node = (FSNode*)node->value;
            if (fs_node && !fs_node->is_directory) {
                stats_out->used_space += fs_node->size;
            }
            node = node->next;
        }
    }
    return OFS_SUCCESS;
}

int dir_exists(OFSInstance* ofs, SessionInfo* session, const char* path) {
    if (!ofs || !session || !path) return OFS_ERROR_INVALID_PARAM;
    FSNode* node = fstree_find(ofs->file_tree, path);
    if (node && node->is_directory) return OFS_SUCCESS;
    return OFS_ERROR_NOT_FOUND;
}