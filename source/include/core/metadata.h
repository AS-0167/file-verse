#ifndef METADATA_HPP
#define METADATA_HPP

#include <string>
#include <vector>
#include "odf_types.hpp"
#include "HashTable.h"
#include "FSNode.h"
#include "FreeSpaceManager.h"
#include "fs_core.h"



class metadata {


private:
    FSInstance* fs;  // Pointer to file system instance

    // Helper: recursively count files/directories for get_stats
    void count_nodes(FSNode* node, uint32_t& files, uint32_t& dirs);

public:
    explicit metadata(FSInstance* fs_instance);

    // Get detailed metadata of a file/directory
    int get_metadata(void* session, const char* path, FileMetadata* meta);

    // Set permissions for a file/directory
    int set_permissions(void* session, const char* path, uint32_t permissions);

    // Get overall file system statistics
    int get_stats(void* session, FSStats* stats);

    // Free memory allocated by file_read
    void free_buffer(void* buffer);

    // Convert error code to string
    const char* get_error_message(int error_code);
    int set_owner(void* session, const std::string& path, const std::string& new_owner) {
    SessionInfo* s = static_cast<SessionInfo*>(session);
    if (!s) return -2; // ERROR_PERMISSION_DENIED
    
    if (s->user.role != UserRole::ADMIN) return -2; // Changed -> to .
    
    cout << "Path:: " << path << endl;
    
    FSNode* node = fs->root->find_node_by_path(path);
    if (!node) {
        cout << "[DEBUG] Node not found for path: " << path << endl;
        return -1; // ERROR_NOT_FOUND
    }
    
    cout << "[DEBUG] Found node: " << node->entry->name << endl;
    
    UserInfo* owner_user = fs->users->get(new_owner);
    if (!owner_user) {
        cout << "[DEBUG] User not found: " << new_owner << endl;
        return -1; // ERROR_NOT_FOUND
    }
    
    cout << "[DEBUG] Changing owner from '" << node->entry->owner 
         << "' to '" << new_owner << "'" << endl;
    
    strncpy(node->entry->owner, new_owner.c_str(), sizeof(node->entry->owner));
    node->entry->owner[sizeof(node->entry->owner)-1] = '\0';
    
    // Give full permissions to new owner
    node->entry->permissions |= static_cast<uint32_t>(FilePermissions::OWNER_READ)   |
                                static_cast<uint32_t>(FilePermissions::OWNER_WRITE)  |
                                static_cast<uint32_t>(FilePermissions::OWNER_EXECUTE);
    
    return 0; // SUCCESS
}
};


#endif // METADATA_HPP
