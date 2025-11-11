
#include "metadata.h"

#include <ctime>
#include <iostream>

MetadataManager::MetadataManager(HashTable<FSNode*>* table, FreeSpaceManager* fsm)
    : pathTable(table), freeSpaceManager(fsm) {}

int MetadataManager::get_metadata(void* session, const char* path, FileMetadata* meta) {
    if (!path || !meta)
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    FSNode* node = pathTable->get(path);
    if (!node)
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

    FileEntry* entry = node->entry;
    meta->path = entry->name;
    meta->owner = entry->owner;
    meta->permissions = entry->permissions;
    meta->size = entry->size;
    meta->createdAt = entry->createdAt;
    meta->modifiedAt = entry->modifiedAt;
    meta->isDirectory = (entry->type == EntryType::DIRECTORY);
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int MetadataManager::set_permissions(void* session, const char* path, uint32_t permissions) {
    if (!path)
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    FSNode* node = pathTable->get(path);
    if (!node)
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

    node->entry->permissions = permissions;
    node->entry->modifiedAt = std::time(nullptr);
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int MetadataManager::get_stats(void* session, FSStats* stats) {
    if (!stats)
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    stats->totalBlocks = freeSpaceManager->getTotalBlocks();
    stats->freeBlocks = freeSpaceManager->getFreeBlocks();
    stats->usedBlocks = stats->totalBlocks - stats->freeBlocks;

    stats->totalFiles = 0;
    stats->totalDirectories = 0;

    pathTable->forEach([&](const std::string&, FSNode* node) {
        if (node->entry->type == EntryType::DIRECTORY)
            stats->totalDirectories++;
        else
            stats->totalFiles++;
    });

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

void MetadataManager::free_buffer(void* buffer) {
    if (buffer)
        delete[] reinterpret_cast<char*>(buffer);
}

const char* MetadataManager::get_error_message(int error_code) {
    switch (static_cast<OFSErrorCodes>(error_code)) {
        case OFSErrorCodes::SUCCESS: return "Operation successful.";
        case OFSErrorCodes::ERROR_NOT_FOUND: return "File or directory not found.";
        case OFSErrorCodes::ERROR_ACCESS_DENIED: return "Access denied.";
        case OFSErrorCodes::ERROR_IO_ERROR: return "I/O error occurred.";
        case OFSErrorCodes::ERROR_INVALID_OPERATION: return "Invalid operation.";
        default: return "Unknown error.";
    }
}
