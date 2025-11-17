#include "dir_manager.h"
#include <iostream>

dir_manager::dir_manager(FSNode* root_node, user_manager* user_mgr)
    : root(root_node), um(user_mgr) {}

FSNode* dir_manager::resolve_path(const string& path) {
    if (path.empty() || path[0] != '/') return nullptr;
    if (path == "/") return root;

    FSNode* current = root;
    size_t start = 1;

    while (start < path.size()) {
        size_t end = path.find('/', start);
        string part = (end == string::npos) ? path.substr(start) : path.substr(start, end - start);
        if (part.empty()) { start = end + 1; continue; }

        current = current->getChild(part);
        if (!current) return nullptr;

        if (end == string::npos) break;
        start = end + 1;
    }
    return current;
}



bool dir_manager::check_dir_permission(void* session, FSNode* node, uint32_t required_perm) {
    if (!session || !node) return false;

    SessionInfo info;
    if (um->get_session_info(session, &info) != 0) return false;

    uint32_t perms = node->entry->permissions;
    const string& owner = node->entry->owner;
    bool user_is_owner = (info.user.username == owner);
    if (info.user.role == UserRole::ADMIN) return true;

    if(!user_is_owner) return false;

    if (info.user.username == node->entry->owner) {
        // Owner bits
        uint32_t owner_bits = 0;
        if (required_perm & static_cast<uint32_t>(FilePermissions::OWNER_READ))
            owner_bits |= static_cast<uint32_t>(FilePermissions::OWNER_READ);
        if (required_perm & static_cast<uint32_t>(FilePermissions::OWNER_WRITE))
            owner_bits |= static_cast<uint32_t>(FilePermissions::OWNER_WRITE);
        if (required_perm & static_cast<uint32_t>(FilePermissions::OWNER_EXECUTE))
            owner_bits |= static_cast<uint32_t>(FilePermissions::OWNER_EXECUTE);

        return (perms & owner_bits) == owner_bits;
    } else {
        // Others bits
        uint32_t others_bits = 0;
        if (required_perm & static_cast<uint32_t>(FilePermissions::OWNER_READ))
            others_bits |= static_cast<uint32_t>(FilePermissions::OTHERS_READ);
        if (required_perm & static_cast<uint32_t>(FilePermissions::OWNER_WRITE))
            others_bits |= static_cast<uint32_t>(FilePermissions::OTHERS_WRITE);
        if (required_perm & static_cast<uint32_t>(FilePermissions::OWNER_EXECUTE))
            others_bits |= static_cast<uint32_t>(FilePermissions::OTHERS_EXECUTE);

        return (perms & others_bits) == others_bits;
    }
}


// -----------------------------------------------------------------------------
// Create new directory
// -----------------------------------------------------------------------------
int dir_manager::dir_create(void* session, const char* path) {
    if (!session || !path)
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    string str_path(path);
    if (str_path.empty() || str_path[0] != '/')
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    size_t slash_pos = str_path.find_last_of('/');
    string parent_path = (slash_pos == 0) ? "/" : str_path.substr(0, slash_pos);
    string dirname = str_path.substr(slash_pos + 1);

    if (dirname.empty())
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    FSNode* parent = resolve_path(parent_path);
    if (!parent)
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

    // Require WRITE permission on parent to create
    if (!check_dir_permission(session, parent, static_cast<uint32_t>(FilePermissions::OWNER_WRITE)))
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    if (parent->getChild(dirname))
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);

    SessionInfo info;
    um->get_session_info(session, &info);

    FileEntry* entry = new FileEntry(dirname, EntryType::DIRECTORY, 0, 0755,
                                     info.user.username, 0);
    FSNode* new_node = new FSNode(entry, parent);
    parent->addChild(new_node);

    cout << "[DEBUG] Directory created: " << path << endl;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// -----------------------------------------------------------------------------
// List all files/directories inside a directory
// -----------------------------------------------------------------------------
int dir_manager::dir_list(void* session, const char* path, FileEntry** entries, int* count) {
    if (!session || !path || !entries || !count)
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    FSNode* dir = resolve_path(path);
    if (!dir)
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

    if (dir->entry->getType() != EntryType::DIRECTORY)
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    // Require READ permission to list
    if (!check_dir_permission(session, dir, static_cast<uint32_t>(FilePermissions::OWNER_READ)))
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    vector<FSNode*> children = dir->getChildren();
    *count = children.size();

    if (*count == 0) {
        *entries = nullptr;
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }

    *entries = new FileEntry[*count];
    for (int i = 0; i < *count; ++i)
        (*entries)[i] = *(children[i]->entry);

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// -----------------------------------------------------------------------------
// Delete a directory (must be empty)
// -----------------------------------------------------------------------------
int dir_manager::dir_delete(void* session, const char* path) {
    if (!session || !path)
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    if (string(path) == "/")
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    FSNode* node = resolve_path(path);
    if (!node)
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

    if (node->entry->getType() != EntryType::DIRECTORY)
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    // Require WRITE permission on parent to delete
    FSNode* parent = node->parent;
    if (!parent)
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    if (!check_dir_permission(session, parent, static_cast<uint32_t>(FilePermissions::OWNER_WRITE)))
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    if (!node->getChildren().empty())
        return static_cast<int>(OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY);

    parent->removeChild(node->entry->name);

    cout << "[DEBUG] Directory deleted: " << path << endl;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// -----------------------------------------------------------------------------
// Check if directory exists
// -----------------------------------------------------------------------------
int dir_manager::dir_exists(void* session, const char* path) {
    if (!path) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    FSNode* dir = resolve_path(path);
    if (!dir) return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    return (dir->entry->getType() == EntryType::DIRECTORY)
           ? static_cast<int>(OFSErrorCodes::SUCCESS)
           : static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

}
