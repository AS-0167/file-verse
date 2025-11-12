#pragma once
#include <memory>
#include <string>
#include "ofs_types.h"
#include "config.h"
#include "../data_structures/hashmap.hpp"
#include "../data_structures/bitmap.hpp"
#include "../data_structures/linked_list.hpp"
#include "../data_structures/dynamic_array.hpp"
#include "../data_structures/fifo_queue.h"

struct DirNode {
    std::string name;
    HashMap<std::string, std::unique_ptr<DirNode>> subdirs;
    DirNode* parent = nullptr;
};

struct UserTable {
    HashMap<std::string, UserInfo> by_name;
};

struct FreeSpace {
    Bitmap blocks; 
};

struct OFS {
    OMNIHeader header{};          
    Config config{};              
    HashMap<std::string, UserInfo> users;  
    std::unique_ptr<DirNode> root; 
    Bitmap free_space;             
    int fd = -1;                   
};


enum class OFSOpType { Ping, Sleep, Unknown };

OFSErrorCodes fs_format(const std::string& omni_path, const Config& cfg, std::string* err = nullptr);
OFSErrorCodes fs_init(std::unique_ptr<OFS>& out, const std::string& omni_path, const std::string& config_path, std::string* err = nullptr);
void fs_shutdown(std::unique_ptr<OFS>& ofs);
