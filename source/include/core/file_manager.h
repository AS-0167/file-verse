#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <string>
#include <vector>
#include "fs_core.h"
#include "HashTable.h"
#include "user_manager.h"
#include "FSNode.h"


struct FSNode;
struct FileEntry;

class file_manager {
private:
    FSInstance* fs_instance; 
    user_manager* um;       

public:
    explicit file_manager(FSInstance* fs, user_manager* user_mgr);
    ~file_manager();

    int file_create(void* session, const char* path, const char* data, size_t size);
    int file_read(void* session, const char* path, char** buffer, size_t* size);
    int file_edit(void* session, const char* path, const char* data, size_t size, uint index);
    int file_delete(void* session, const char* path);
    int file_truncate(void* session, const char* path);
    int file_exists(void* session, const char* path);
    int file_rename(void* session, const char* old_path, const char* new_path);

private:
    FSNode* resolve_path(const std::string& path);    
    bool check_permissions(void* session, FSNode* node);

};

#endif // FILE_MANAGER_H

