#include "dir_manager.h"
#include <iostream>

dir_manager::dir_manager(FSNode* root_node, user_manager* user_mgr)
    : root(root_node), um(user_mgr) {}

FSNode* dir_manager::resolve_path(const string& path) {
    if (path == "/") return root;

    FSNode* current = root;
    size_t start = 1;  // Skip initial '/'
    while (start < path.size()) {
        size_t end = path.find('/', start);
        string part = (end == string::npos) ? path.substr(start) : path.substr(start, end - start);
        if (part.empty()) break;

        current = current->getChild(part);
        if (!current) return nullptr;

        if (end == string::npos) break;
        start = end + 1;
    }
    return current;
}

bool dir_manager::check_permissions(void* session, FSNode* node) {
    if (!node || !session) return false;

    SessionInfo info;
    if (um->get_session_info(session, &info) != 0)
        return false;

    return (std::strcmp(info.user.username, node->entry->owner) == 0 ||  info.user.role == UserRole::ADMIN);
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

    if (!check_permissions(session, parent))
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


    if (!check_permissions(session, dir))
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


    if (!check_permissions(session, node))
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    if (!node->getChildren().empty())
        return static_cast<int>(OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY);

    FSNode* parent = node->parent;

    if (parent)
        parent->removeChild(node->entry->name);

    delete node;
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
