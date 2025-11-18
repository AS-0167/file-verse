#include "metadata.h"
#include <cstring>
#include <iostream>

metadata::metadata(FSInstance* fs_instance) : fs(fs_instance) {}

// -------------------------- Helper --------------------------
void metadata::count_nodes(FSNode* node, uint32_t& files, uint32_t& dirs) {
    if (!node) return;
    if (node->entry->getType() == EntryType::FILE) files++;
    else if (node->entry->getType() == EntryType::DIRECTORY) dirs++;


    for (FSNode* child : node->getChildren())
        count_nodes(child, files, dirs);
}

// -------------------------- get_metadata --------------------------
int metadata::get_metadata(void* session, const char* path, FileMetadata* meta) {
    if (!fs || !path || !meta) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    FSNode* node = fs->root; // start from root
    // Traverse the path manually
    std::string p(path);
    if (p != "/") {
        size_t start = 1; // skip leading '/'
        while (start < p.size()) {
            size_t end = p.find('/', start);
            std::string part = p.substr(start, end - start);
            node = node->getChild(part);
            if (!node) return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    meta->entry = *(node->entry);
    meta->blocks_used = (node->entry->size + fs->header.block_size - 1) / fs->header.block_size;

    //*meta = FileMetadata(path, *(node->entry));
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// -------------------------- set_permissions --------------------------
int metadata::set_permissions(void* session, const char* path, uint32_t permissions) {
    if (!fs || !path) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    FSNode* node = fs->root;
    std::string p(path);
    if (p != "/") {
        size_t start = 1;
        while (start < p.size()) {
            size_t end = p.find('/', start);
            std::string part = p.substr(start, end - start);
            node = node->getChild(part);
            if (!node) return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }

    node->entry->permissions = permissions;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// -------------------------- get_stats --------------------------
int metadata::get_stats(void* session, FSStats* stats) {
    if (!fs || !stats) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    stats->total_size = fs->header.total_size;
    stats->used_space = 0;  // Compute used space by summing file sizes
    stats->free_space = fs->header.total_size;

    uint32_t files = 0, dirs = 0;
    count_nodes(fs->root, files, dirs);
    stats->total_files = files;
    stats->total_directories = dirs;

    // Count users
 // Count users
    stats->total_users = 0;
    if (fs->users) {
    auto buckets = fs->users->getBuckets();
    for (auto head : buckets) {
        auto node = head;
        while (node) {
            if (node->value && node->value->is_active)
                stats->total_users++;  // increment directly
            node = node->next;
        }
    }
    }
    stats->total_users--;  



    // Active sessions
    stats->active_sessions = fs->sessions.size();

    // Fragmentation placeholder (not implemented)
    stats->fragmentation = 0.0;

    // Calculate used/free space (sum of file sizes)
    for (FSNode* child : fs->root->getChildren()) {
        if (child->entry->getType() == EntryType::FILE) stats->used_space += child->entry->size;
        // For nested files, a full recursive sum can be added
    }
    stats->free_space = fs->header.total_size > stats->used_space ? 
                        fs->header.total_size - stats->used_space : 0;

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// -------------------------- free_buffer --------------------------
void metadata::free_buffer(void* buffer) {
    if (buffer) delete[] static_cast<char*>(buffer);
}

// -------------------------- get_error_message --------------------------
const char* metadata::get_error_message(int error_code) {
    switch (static_cast<OFSErrorCodes>(error_code)) {
        case OFSErrorCodes::SUCCESS: return "SUCCESS";
        case OFSErrorCodes::ERROR_NOT_FOUND: return "ERROR_NOT_FOUND";
        case OFSErrorCodes::ERROR_PERMISSION_DENIED: return "ERROR_PERMISSION_DENIED";
        case OFSErrorCodes::ERROR_IO_ERROR: return "ERROR_IO_ERROR";
        case OFSErrorCodes::ERROR_INVALID_PATH: return "ERROR_INVALID_PATH";
        case OFSErrorCodes::ERROR_FILE_EXISTS: return "ERROR_FILE_EXISTS";
        case OFSErrorCodes::ERROR_NO_SPACE: return "ERROR_NO_SPACE";
        case OFSErrorCodes::ERROR_INVALID_CONFIG: return "ERROR_INVALID_CONFIG";
        case OFSErrorCodes::ERROR_NOT_IMPLEMENTED: return "ERROR_NOT_IMPLEMENTED";
        case OFSErrorCodes::ERROR_INVALID_SESSION: return "ERROR_INVALID_SESSION";
        case OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY: return "ERROR_DIRECTORY_NOT_EMPTY";
        case OFSErrorCodes::ERROR_INVALID_OPERATION: return "ERROR_INVALID_OPERATION";
        default: return "UNKNOWN_ERROR";
    }
}
