#ifndef DIR_MANAGER_H
#define DIR_MANAGER_H

#include <string>
#include <vector>
#include "FSNode.h"
#include "user_manager.h"
#include "odf_types.hpp"

using namespace std;

class dir_manager {
private:
    FSNode* root;
    user_manager* um;

    FSNode* resolve_path(const string& path);
    bool check_permissions(void* session, FSNode* node, uint32_t required_perms) ;
    bool check_dir_permission(void* session, FSNode* node, uint32_t required_perm);
    

public:
    dir_manager(FSNode* root_node, user_manager* user_mgr);

    int dir_create(void* session, const char* path);
    int dir_list(void* session, const char* path, FileEntry** entries, int* count);
    int dir_delete(void* session, const char* path);
    int dir_exists(void* session, const char* path);
};

#endif
