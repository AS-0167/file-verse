#ifndef FS_CORE_H
#define FS_CORE_H

#include <string>
#include <vector>
#include "odf_types.hpp"
#include "HashTable.h"
#include "FSNode.h"
#include "FreeSpaceManager.h"

using namespace std;
struct FSInstance {
    std::string omni_path;
    OMNIHeader header;
    HashTable<UserInfo>* users;
    FSNode* root;
    FreeSpaceManager* fsm;
    vector<void*> sessions;
    uint next_file_index; 
};


int fs_format(const char* omni_path, const char* config_path);
int fs_init(void** instance, const char* omni_path, const char* config_path);
void fs_shutdown(void* instance);

#endif // FS_CORE_H


//CHanged COde 

/*
#ifndef FS_CORE_H
#define FS_CORE_H

#include <string>
#include <vector>
#include "odf_types.hpp"
#include "HashTable.h"
#include "FSNode.h"
#include "FreeSpaceManager.h"

using namespace std;
struct FSInstance {
    std::string omni_path;
    OMNIHeader header;
    HashTable<UserInfo>* users;
    FSNode* root;
    FreeSpaceManager* fsm;
    vector<void*> sessions;
    uint next_file_index; 
    uint64_t data_blocks_start;
};


int fs_format(const char* omni_path, const char* config_path);
int fs_init(void** instance, const char* omni_path, const char* config_path);
void fs_shutdown(void* instance);

#endif // FS_CORE_H
*/