#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h> // For uint32_t data types

#include "ofs_types.h"
#include "fs_tree.h"
#include "bitmap.h"

// ============================================================================
// file_create - Create a new file with initial data
// ============================================================================
int file_create(OFSInstance* ofs, SessionInfo* session, const char* path, const char* data, size_t size) {
    if (!ofs || !session || !path) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 1. Check if something already exists at this path
    if (fstree_find(ofs->file_tree, path) != NULL) {
        return OFS_ERROR_ALREADY_EXISTS;
    }

    // 2. Find the parent directory
    char parent_path[MAX_PATH_LENGTH];
    strncpy(parent_path, path, sizeof(parent_path) - 1);
    char* last_slash = strrchr(parent_path, '/');
    if (!last_slash || last_slash == parent_path) {
        strcpy(parent_path, "/");
    } else {
        *last_slash = '\0';
    }

    FSNode* parent_node = fstree_find(ofs->file_tree, parent_path);
    if (!parent_node || !parent_node->is_directory) {
        return OFS_ERROR_NOT_FOUND; // Parent directory doesn't exist
    }

    // 3. Find a free block in the bitmap for the file's content
    int block_index = bitmap_find_first_free(ofs->free_blocks);
    if (block_index == -1) {
        return OFS_ERROR_NO_SPACE;
    }
    
    // Check if the data will fit in a single block (minus the 4-byte pointer)
    if (size > ofs->header.block_size - sizeof(uint32_t)) {
        return OFS_ERROR_FILE_TOO_LARGE;
    }

    // 4. Mark the block as used
    bitmap_set(ofs->free_blocks, block_index);

    // 5. Write the file content to the block in the .omni file
    fseek(ofs->omni_file, ofs->header.content_offset + (block_index * ofs->header.block_size), SEEK_SET);
    // Write the block linking pointer (0 for the last/only block)
    uint32_t next_block = 0;
    fwrite(&next_block, sizeof(uint32_t), 1, ofs->omni_file);
    // Write the actual data
    if (data && size > 0) {
        fwrite(data, 1, size, ofs->omni_file);
    }

    // 6. Create the metadata entry for the new file (for disk)
    FileEntry new_entry = {0};
    new_entry.is_valid = 0;
    new_entry.is_directory = 0; // It's a file
    new_entry.parent_index = parent_node->entry_index;
    const char* name = strrchr(path, '/') + 1;
    strncpy(new_entry.name, name, 11);
    new_entry.name[11] = '\0';
    new_entry.entry_index = ofs->next_entry_index++;
    new_entry.owner_id = session->user_id;
    new_entry.start_block = block_index;
    new_entry.total_size = size;
    new_entry.created_time = time(NULL);
    new_entry.modified_time = new_entry.created_time;

    // 7. Write the new metadata entry to the .omni file
    fseek(ofs->omni_file, ofs->header.metadata_offset + (new_entry.entry_index * sizeof(FileEntry)), SEEK_SET);
    fwrite(&new_entry, sizeof(FileEntry), 1, ofs->omni_file);
    fflush(ofs->omni_file);

    // 8. Create the in-memory node and add it to the tree
    FSNode* new_node = fsnode_create(new_entry.name, 0); // is_directory = 0
    new_node->entry_index = new_entry.entry_index;
    new_node->owner_id = new_entry.owner_id;
    new_node->created_time = new_entry.created_time;
    new_node->modified_time = new_entry.modified_time;
    new_node->start_block = new_entry.start_block;
    new_node->size = new_entry.total_size;
     printf("DEBUG: Calling fstree_add_node. ofs_ptr=%p, file_tree_ptr=%p, path='%s', new_node_ptr=%p\n",
           (void*)ofs, (void*)ofs->file_tree, path, (void*)new_node);

    fstree_add_node(ofs->file_tree, path, new_node);

    printf("FILE_CREATE: Successfully created file '%s' in block %d\n", path, block_index);
    return OFS_SUCCESS;
}

// ============================================================================
// file_read - Read the content of a file
// ============================================================================
int file_read(OFSInstance* ofs, SessionInfo* session, const char* path, char** buffer_out, size_t* size_out) {
    if (!ofs || !session || !path || !buffer_out || !size_out) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 1. Find the file node in the in-memory tree
    FSNode* file_node = fstree_find(ofs->file_tree, path);
    if (!file_node || file_node->is_directory) {
        return OFS_ERROR_NOT_FOUND;
    }

    // 2. Allocate a buffer to hold the file content
    *size_out = file_node->size;
    if (*size_out == 0) {
        *buffer_out = NULL;
        return OFS_SUCCESS; // File is empty
    }

    *buffer_out = (char*)malloc(*size_out + 1); // +1 for null terminator
    if (!(*buffer_out)) {
        return OFS_ERROR_SYSTEM;
    }

    // 3. Seek to the file's data block and read the content
    fseek(ofs->omni_file, ofs->header.content_offset + (file_node->start_block * ofs->header.block_size), SEEK_SET);
    // Skip the 4-byte block linking pointer
    fseek(ofs->omni_file, sizeof(uint32_t), SEEK_CUR); 
    fread(*buffer_out, 1, *size_out, ofs->omni_file);

    // 4. Null-terminate the buffer to be safe
    (*buffer_out)[*size_out] = '\0';

    printf("FILE_READ: Successfully read %zu bytes from file '%s'\n", *size_out, path);
    return OFS_SUCCESS;
}

// ============================================================================
// file_delete - Deletes a file
// ============================================================================
int file_delete(OFSInstance* ofs, SessionInfo* session, const char* path) {
    if (!ofs || !session || !path) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 1. Find the file node in the in-memory tree
    FSNode* node_to_delete = fstree_find(ofs->file_tree, path);

    // 2. Check if the node exists and is a file (not a directory)
    if (!node_to_delete || node_to_delete->is_directory) {
        return OFS_ERROR_NOT_FOUND;
    }

    // 3. Free the data blocks used by the file
    printf("FILE_DELETE: Freeing data blocks for file '%s' starting at block %u\n", 
           path, node_to_delete->start_block);
    
    uint32_t current_block = node_to_delete->start_block;
    // This loop is for future-proofing; our simple system only uses one block per file for now
    while (current_block != 0 && current_block < ofs->header.total_blocks) {
        bitmap_clear(ofs->free_blocks, current_block);
        
        fseek(ofs->omni_file, ofs->header.content_offset + (current_block * ofs->header.block_size), SEEK_SET);
        uint32_t next_block = 0;
        fread(&next_block, sizeof(uint32_t), 1, ofs->omni_file);
        
        printf("  - Freed block %u. Next block is %u.\n", current_block, next_block);
        current_block = next_block;
    }

    // 4. Invalidate the metadata entry on disk
    uint32_t entry_index = node_to_delete->entry_index;
    printf("FILE_DELETE: Invalidating metadata entry at index %u\n", entry_index);
    
    FileEntry invalid_entry = {0};
    invalid_entry.is_valid = 1; // 1 means invalid/free
    
    fseek(ofs->omni_file, ofs->header.metadata_offset + (entry_index * sizeof(FileEntry)), SEEK_SET);
    fwrite(&invalid_entry, sizeof(FileEntry), 1, ofs->omni_file);
    fflush(ofs->omni_file);

    // 5. Remove the node from the in-memory tree structures
    fstree_remove(ofs->file_tree, path);

    // 6. Free the memory of the disconnected FSNode
    fsnode_destroy(node_to_delete);
    
    return OFS_SUCCESS;
}




// ============================================================================
// file_edit - (DEBUGGING VERSION with extensive logging)
// ============================================================================
int file_edit(OFSInstance* ofs, SessionInfo* session, const char* path, const char* data, size_t size, uint32_t index) {

    if (!ofs || !session || !path || !data)
        return OFS_ERROR_INVALID_PARAM;

    // 1. Find the file node and validate it
    FSNode* node = fstree_find(ofs->file_tree, path);
    if (!node)
        return OFS_ERROR_NOT_FOUND;
    if (node->is_directory)
        return OFS_ERROR_NOT_A_FILE;

    // 2. Calculate new size
    uint32_t new_size = node->size;
    if (index + size > node->size)
        new_size = index + size;

    uint32_t max_file_size = ofs->header.block_size - sizeof(uint32_t);
    if (new_size > max_file_size)
        return OFS_ERROR_FILE_TOO_LARGE;

    // 3. Read existing block content into buffer
    char* block_buffer = (char*)malloc(max_file_size);
    if (!block_buffer)
        return OFS_ERROR_SYSTEM;

    memset(block_buffer, 0, max_file_size);

    uint64_t data_start_offset =
        ofs->header.content_offset +
        (node->start_block * ofs->header.block_size) +
        sizeof(uint32_t);

    if (node->size > 0) {
        fseek(ofs->omni_file, data_start_offset, SEEK_SET);
        fread(block_buffer, 1, node->size, ofs->omni_file);
    }

    // 4. Modify buffer
    memcpy(block_buffer + index, data, size);

    // 5. Write buffer back to disk
    fseek(ofs->omni_file, data_start_offset, SEEK_SET);
    size_t bytes_written = fwrite(block_buffer, 1, new_size, ofs->omni_file);
    free(block_buffer);

    fflush(ofs->omni_file);

    if (bytes_written != new_size)
        return OFS_ERROR_IO;

    // 6. Update in-memory metadata
    node->size = new_size;
    node->modified_time = time(NULL);

    // 7. Update on-disk metadata
    uint64_t metadata_offset =
        ofs->header.metadata_offset +
        (node->entry_index * sizeof(FileEntry));

    FileEntry entry_on_disk;
    fseek(ofs->omni_file, metadata_offset, SEEK_SET);
    fread(&entry_on_disk, sizeof(FileEntry), 1, ofs->omni_file);

    entry_on_disk.total_size = node->size;
    entry_on_disk.modified_time = node->modified_time;

    fseek(ofs->omni_file, metadata_offset, SEEK_SET);
    fwrite(&entry_on_disk, sizeof(FileEntry), 1, ofs->omni_file);
    fflush(ofs->omni_file);

    return OFS_SUCCESS;
}



// ============================================================================
// file_rename - Renames or moves a file
// ============================================================================
int file_rename(OFSInstance* ofs, SessionInfo* session, const char* old_path, const char* new_path) {
    printf("\n--- FILE_RENAME: Attempting to move '%s' to '%s' ---\n", old_path, new_path);

    // 1. --- VALIDATE INPUTS ---
    if (!ofs || !session || !old_path || !new_path) {
        return OFS_ERROR_INVALID_PARAM;
    }
    // Renaming to the same thing is a success (no work needed)
    if (strcmp(old_path, new_path) == 0) {
        return OFS_SUCCESS;
    }

    // 2. --- FIND THE SOURCE NODE ---
    FSNode* node_to_move = fstree_find(ofs->file_tree, old_path);
    if (!node_to_move) {
        printf("FILE_RENAME_ERROR: Source path '%s' not found.\n", old_path);
        return OFS_ERROR_NOT_FOUND;
    }
    if (node_to_move->is_directory) {
        printf("FILE_RENAME_ERROR: Source path '%s' is a directory (use dir_rename).\n", old_path);
        return OFS_ERROR_NOT_A_FILE;
    }

    // 3. --- CHECK FOR DESTINATION COLLISION ---
    if (fstree_find(ofs->file_tree, new_path) != NULL) {
        printf("FILE_RENAME_ERROR: Destination path '%s' already exists.\n", new_path);
        return OFS_ERROR_ALREADY_EXISTS;
    }

    // 4. --- FIND THE NEW PARENT DIRECTORY ---
    char new_parent_path[MAX_PATH_LENGTH];
    strncpy(new_parent_path, new_path, sizeof(new_parent_path) - 1);
    new_parent_path[sizeof(new_parent_path) - 1] = '\0'; // Ensure null termination

    char* last_slash = strrchr(new_parent_path, '/');
    if (!last_slash) {
        return OFS_ERROR_INVALID_PARAM; // Path format is invalid
    }

    // Extract the new filename from the new_path
    const char* new_name = last_slash + 1;
    if (strlen(new_name) > 11) {
        printf("FILE_RENAME_ERROR: New filename '%s' is longer than 11 characters.\n", new_name);
        return OFS_ERROR_INVALID_PARAM;
    }

    // Determine the path of the new parent directory
    if (last_slash == new_parent_path) { // New parent is the root directory
        strcpy(new_parent_path, "/");
    } else {
        *last_slash = '\0'; // Cut the string at the slash to get the parent path
    }

    FSNode* new_parent_node = fstree_find(ofs->file_tree, new_parent_path);
    if (!new_parent_node || !new_parent_node->is_directory) {
        printf("FILE_RENAME_ERROR: Destination directory '%s' not found.\n", new_parent_path);
        return OFS_ERROR_NOT_FOUND;
    }

    // 5. --- PERFORM IN-MEMORY TREE MANIPULATION ---
    // Unlink the node from its old parent. This does NOT free the node's memory.
    if (fstree_remove(ofs->file_tree, old_path) != 0) {
        printf("FILE_RENAME_ERROR: Critical error unlinking node from tree.\n");
        return OFS_ERROR_SYSTEM;
    }

    // Update the node's name in memory to its new name
    strncpy(node_to_move->name, new_name, 11);
    node_to_move->name[11] = '\0';

    // Add the very same node back to the tree at its new path
    if (fstree_add_node(ofs->file_tree, new_path, node_to_move) != 0) {
        // This is a critical failure. A robust system might try to roll back.
        printf("FILE_RENAME_ERROR: Critical error re-linking node in tree.\n");
        return OFS_ERROR_SYSTEM;
    }
    printf("FILE_RENAME: In-memory tree updated successfully.\n");

    // 6. --- UPDATE ON-DISK METADATA ---
    // The actual file content block does not move, only its metadata entry changes.
    uint64_t metadata_offset = ofs->header.metadata_offset + (node_to_move->entry_index * sizeof(FileEntry));

    FileEntry entry_on_disk;
    fseek(ofs->omni_file, metadata_offset, SEEK_SET);
    fread(&entry_on_disk, sizeof(FileEntry), 1, ofs->omni_file);

    // Update the name, parent pointer, and modification time
    strncpy(entry_on_disk.name, new_name, 11);
    entry_on_disk.name[11] = '\0';
    entry_on_disk.parent_index = new_parent_node->entry_index;
    entry_on_disk.modified_time = time(NULL);

    // Write the updated metadata entry back to the same location
    fseek(ofs->omni_file, metadata_offset, SEEK_SET);
    fwrite(&entry_on_disk, sizeof(FileEntry), 1, ofs->omni_file);
    fflush(ofs->omni_file);

    printf("FILE_RENAME: On-disk metadata for entry %u updated successfully.\n", node_to_move->entry_index);
    printf("--- FILE_RENAME: Move completed successfully. ---\n\n");

    return OFS_SUCCESS;
}




// ============================================================================
// get_metadata - Retrieves detailed information about a file or directory
// ============================================================================
// Note: We are using the FileEntry struct as our FileMetadata container.
int get_metadata(OFSInstance* ofs, SessionInfo* session, const char* path, FileEntry* meta_out) {
    // 1. --- VALIDATE INPUTS ---
    if (!ofs || !session || !path || !meta_out) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 2. --- FIND THE NODE IN THE FILE TREE ---
    FSNode* node = fstree_find(ofs->file_tree, path);
    if (!node) {
        return OFS_ERROR_NOT_FOUND;
    }
    
    // 3. --- PERMISSION CHECK ---
    // For now, we'll allow any valid user to get metadata.
    // A more secure system might restrict this based on directory permissions.
    if (!session->is_valid) {
        return OFS_ERROR_SESSION_INVALID;
    }

    // 4. --- COPY METADATA FROM THE IN-MEMORY FSNODE ---
    // We construct the metadata from the live, in-memory node, which is always up-to-date.
    memset(meta_out, 0, sizeof(FileEntry)); // Clear the output struct first
    
    meta_out->is_valid = 1; // 1 means valid in this context
    meta_out->is_directory = node->is_directory;
    meta_out->entry_index = node->entry_index;
    strncpy(meta_out->name, node->name, 11);
    meta_out->name[11] = '\0';
    
    meta_out->total_size = node->size;
    meta_out->owner_id = node->owner_id;
    meta_out->permissions = node->permissions;
    meta_out->created_time = node->created_time;
    meta_out->modified_time = node->modified_time;
    
    if (node->parent) {
        meta_out->parent_index = node->parent->entry_index;
    } else {
        meta_out->parent_index = 0; // The root has no parent
    }

    return OFS_SUCCESS;
}


// ============================================================================
// set_permissions - Changes the permission flags for a file or directory
// ============================================================================
int set_permissions(OFSInstance* ofs, SessionInfo* session, const char* path, uint32_t permissions) {
    // 1. --- VALIDATE INPUTS ---
    if (!ofs || !session || !path) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 2. --- FIND THE NODE IN THE FILE TREE ---
    FSNode* node = fstree_find(ofs->file_tree, path);
    if (!node) {
        return OFS_ERROR_NOT_FOUND;
    }

    // 3. --- PERMISSION CHECK ---
    // The user must be the owner of the file OR an administrator.
    if (node->owner_id != session->user_id && session->role != ROLE_ADMIN) {
        printf("--- SECURITY: User %u (not owner) tried to change permissions on file owned by %u. Denied. ---\n", session->user_id, node->owner_id);
        return OFS_ERROR_PERMISSION_DENIED;
    }

    // 4. --- UPDATE IN-MEMORY FSNODE ---
    node->permissions = permissions;
    node->modified_time = time(NULL); // Changing permissions updates the modification time

    // 5. --- UPDATE ON-DISK FILEENTRY ---
    uint64_t metadata_offset = ofs->header.metadata_offset + (node->entry_index * sizeof(FileEntry));
    
    FileEntry entry_on_disk;
    // Seek and read the current entry
    fseek(ofs->omni_file, metadata_offset, SEEK_SET);
    if (fread(&entry_on_disk, sizeof(FileEntry), 1, ofs->omni_file) != 1) {
        return OFS_ERROR_IO; // Failed to read the entry
    }

    // Update the fields
    entry_on_disk.permissions = node->permissions;
    entry_on_disk.modified_time = node->modified_time;

    // Seek back and write the updated entry
    fseek(ofs->omni_file, metadata_offset, SEEK_SET);
    if (fwrite(&entry_on_disk, sizeof(FileEntry), 1, ofs->omni_file) != 1) {
        return OFS_ERROR_IO; // Failed to write the updated entry
    }
    fflush(ofs->omni_file);

    printf("--- PERMS: Permissions for '%s' set to %u by user %u. ---\n", path, permissions, session->user_id);
    return OFS_SUCCESS;
}

// ============================================================================
// file_truncate - Sets the size of a file to 0, erasing its content.
// ============================================================================
int file_truncate(OFSInstance* ofs, SessionInfo* session, const char* path) {
    // 1. --- VALIDATE INPUTS ---
    if (!ofs || !session || !path) {
        return OFS_ERROR_INVALID_PARAM;
    }

    // 2. --- FIND THE NODE AND VALIDATE ---
    FSNode* node = fstree_find(ofs->file_tree, path);
    if (!node) {
        return OFS_ERROR_NOT_FOUND;
    }
    if (node->is_directory) {
        return OFS_ERROR_IS_DIRECTORY; // Cannot truncate a directory
    }

    // 3. --- PERMISSION CHECK ---
    // The user must be the owner of the file OR an administrator.
    if (node->owner_id != session->user_id && session->role != ROLE_ADMIN) {
        return OFS_ERROR_PERMISSION_DENIED;
    }

    // 4. --- FREE THE DATA BLOCKS ---
    // This part is similar to file_delete, but we don't delete the metadata.
    // We find all blocks used by the file and mark them as free in the bitmap.
    // For our simple system, we assume only one block per file.
    if (node->start_block != 0) { // Check if it has any blocks allocated
        bitmap_clear(ofs->free_blocks, node->start_block);
        // A multi-block implementation would loop through the block chain here.
    }
    
    // 5. --- UPDATE IN-MEMORY FSNODE ---
    node->size = 0;
    // The start_block is no longer valid as we've freed it.
    // Some systems set it to 0, some leave it (it will be overwritten on next write).
    // Setting it to 0 is cleaner.
    node->start_block = 0; 
    node->modified_time = time(NULL);

    // 6. --- UPDATE ON-DISK FILEENTRY ---
    uint64_t metadata_offset = ofs->header.metadata_offset + (node->entry_index * sizeof(FileEntry));
    
    FileEntry entry_on_disk;
    fseek(ofs->omni_file, metadata_offset, SEEK_SET);
    fread(&entry_on_disk, sizeof(FileEntry), 1, ofs->omni_file);

    // Update the fields on the disk copy
    entry_on_disk.total_size = 0;
    entry_on_disk.start_block = 0;
    entry_on_disk.modified_time = node->modified_time;

    // Write the updated entry back
    fseek(ofs->omni_file, metadata_offset, SEEK_SET);
    fwrite(&entry_on_disk, sizeof(FileEntry), 1, ofs->omni_file);
    fflush(ofs->omni_file);

    printf("--- TRUNCATE: File '%s' was truncated to 0 bytes by user %u. ---\n", path, session->user_id);
    return OFS_SUCCESS;
}