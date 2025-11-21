#include "file_manager.h"
#include <cstring>
#include <iostream>

using namespace std;

file_manager::file_manager(FSInstance* fs, user_manager* user_mgr)
    : fs_instance(fs), um(user_mgr) {
    // Ensure root exists
    if (!fs_instance->root) {
        FileEntry* root_entry = new FileEntry("/", EntryType::DIRECTORY, 0, 0755, "admin", 0);
        fs_instance->root = new FSNode(root_entry, nullptr);
    }
    if (fs_instance->next_file_index == 0)
        fs_instance->next_file_index = 1;
}

file_manager::~file_manager() = default;

// ------------------ Utility ------------------

FSNode* file_manager::resolve_path(const std::string& path) {
    if (path.empty() || path[0] != '/') return nullptr;

    FSNode* node = fs_instance->root;
    size_t start = 1;

    while (start < path.size()) {
        size_t end = path.find('/', start);
        string token = path.substr(start, end - start);
        if (token.empty()) break;

        node = node->getChild(token);
        if (!node) return nullptr;

        if (end == string::npos) break;
        start = end + 1;
    }
    return node;
}

bool file_manager::check_permissions(void* session, FSNode* node) {
    if (!node || !session) return false;

    SessionInfo info;
    if (um->get_session_info(session, &info) != 0) return false;

    // Admin bypass
    if (info.user.role == UserRole::ADMIN) return true;

    // Compare username with owner
    return std::string(info.user.username) == std::string(node->entry->owner);
}

// ------------------ File Operations ------------------

int file_manager::file_create(void* session, const char* path, const char* data, size_t size) {
    if (!session || !path) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    
    SessionInfo info;
    if (um->get_session_info(session, &info) != 0)
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    
    string str_path(path);
    size_t slash_pos = str_path.find_last_of('/');
    string parent_path = (slash_pos == 0) ? "/" : str_path.substr(0, slash_pos);
    string filename = str_path.substr(slash_pos + 1);
    
    // Validate filename
    if (filename.empty()) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    
    FSNode* parent = resolve_path(parent_path);
    if (!parent) return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    
    // Check if parent is a directory (compare uint8_t values)
    if (parent->entry->type != static_cast<uint8_t>(EntryType::DIRECTORY))
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    
    if (!check_permissions(session, parent)) 
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    
    if (parent->getChild(filename)) 
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
    
    // Get uid from username (or use a default if not available)
    uint32_t uid = 0;  // You may need to look up uid from username
    
    FileEntry* entry = new FileEntry(filename, EntryType::FILE, 
                                     uid,
                                     0644,
                                     info.user.username, 
                                     fs_instance->next_file_index++);
    
    FSNode* new_node = new FSNode(entry, parent);
    parent->addChild(new_node);
    
    if (data && size > 0)
        new_node->data.assign(data, data + size);
    
    new_node->entry->size = new_node->data.size();
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int file_manager::file_read(void* session, const char* path, char** buffer, size_t* size) {
    FSNode* node = resolve_path(path);
    if (!node || node->entry->getType() != EntryType::FILE) 
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

    *size = node->data.size();
    *buffer = new char[*size];
    memcpy(*buffer, node->data.data(), *size);

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int file_manager::file_edit(void* session, const char* path, const char* data, size_t size, uint index) {
    FSNode* node = resolve_path(path);
    if (!node || !check_permissions(session, node)) 
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    if (index > node->data.size()) 
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    if (index + size > node->data.size())
        node->data.resize(index + size);

    memcpy(node->data.data() + index, data, size);
    node->entry->size = node->data.size();
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int file_manager::file_delete(void* session, const char* path) {
    FSNode* node = resolve_path(path);
    if (!node || !check_permissions(session, node))
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    FSNode* parent = node->parent;
    if (!parent) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    // removeChild deletes the node internally
    parent->removeChild(std::string(node->entry->name));

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}



int file_manager::file_truncate(void* session, const char* path) {
    FSNode* node = resolve_path(path);
    if (!node || !check_permissions(session, node)) 
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    node->data.clear();
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int file_manager::file_exists(void* session, const char* path) {
    FSNode* node = resolve_path(path);
    return node ? static_cast<int>(OFSErrorCodes::SUCCESS) : static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
}

int file_manager::file_rename(void* session, const char* old_path, const char* new_path) {
    FSNode* node = resolve_path(old_path);
    if (!node || !check_permissions(session, node)) 
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    size_t slash_pos = string(new_path).find_last_of('/');
    string new_parent_path = (slash_pos == 0) ? "/" : string(new_path).substr(0, slash_pos);
    string new_name = string(new_path).substr(slash_pos + 1);

    FSNode* new_parent = resolve_path(new_parent_path);
    if (!new_parent) return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

    // Check if target already exists in new parent
    if (new_parent->getChild(new_name))
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);

    // Save old parent
    FSNode* old_parent = node->parent;
    
    // Detach from old parent BEFORE changing name
    old_parent->detachChild(node->entry->name);
    
    // Update the name
    std::strncpy(node->entry->name, new_name.c_str(), sizeof(node->entry->name) - 1);
    node->entry->name[sizeof(node->entry->name) - 1] = '\0';
    
    // Attach to new parent with new name
    node->parent = new_parent;
    new_parent->addChild(node);

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}



