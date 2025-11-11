#ifndef FS_CORE_H
#define FS_CORE_H

#include <string>
#include <vector>
#include "odf_types.hpp"
#include "HashTable.h"
#include "FSNode.h"
#include "FreeSpaceManager.h"


struct FSInstance {
    std::string omni_path;
    OMNIHeader header;
    HashTable<UserInfo>* users;
    FSNode* root;
    FreeSpaceManager* fsm;
    std::vector<void*> sessions;
};


int fs_format(const char* omni_path, const char* config_path);
int fs_init(void** instance, const char* omni_path, const char* config_path);
void fs_shutdown(void* instance);

#endif // FS_CORE_H
