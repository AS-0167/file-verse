#ifndef FS_CORE_H
#define FS_CORE_H

#include <string>
#include <vector>
#include "odf_types.hpp"
#include "FSNode.h"
#include "HashTable.h"
#include "FreeSpaceManager.h"

struct FSInstance {
    std::string omni_path;
    OMNIHeader header;
    HashTable<UserInfo>* users;
    FSNode* root;
    FreeSpaceManager* fsm;
    std::vector<void*> sessions; 
};

// Core system functions
int fs_format(const char* omni_path, const char* config_path);
int fs_init(void** instance, const char* omni_path, const char* config_path);
void fs_shutdown(void* instance);

// Helpers for FS tree serialization
int serialize_fs_tree(FSNode* node, std::ofstream& ofs);
FSNode* load_fs_tree(std::ifstream& ifs, uint64_t& offset, uint64_t end_offset);

#endif
