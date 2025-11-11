#include "fs_core.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <ctime>

int serialize_fs_tree(FSNode* node, std::ofstream& ofs) {
    if (!node) return 0;

        ofs.write(reinterpret_cast<const char*>(node->entry), sizeof(FileEntry));

    
    if (node->entry->getType() == EntryType::DIRECTORY && node->children) {
        auto child_node = node->children->getHead();
        while (child_node) {
            serialize_fs_tree(child_node->data, ofs);
            child_node = child_node->next;
        }
    }
    return 0;
}

FSNode* load_fs_tree(std::ifstream& ifs, uint64_t& offset, uint64_t end_offset) {
    if (offset + sizeof(FileEntry) > end_offset) return nullptr;

    FileEntry entry;
    ifs.seekg(offset, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(&entry), sizeof(FileEntry));
    offset += sizeof(FileEntry);

    FSNode* node = new FSNode(new FileEntry(entry));

    if (entry.getType() == EntryType::DIRECTORY) {
        while (offset + sizeof(FileEntry) <= end_offset) {
            FSNode* child = load_fs_tree(ifs, offset, end_offset);
            if (!child) break;
            node->addChild(child);
        }
    }

    return node;
}


int fs_format(const char* omni_path, const char* config_path) {
    std::ofstream ofs(omni_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);


    OMNIHeader header(0x00010000, 1024ULL * 1024 * 50, sizeof(OMNIHeader), 4096);
    std::strncpy(header.magic, "OMNIFS01", sizeof(header.magic) - 1);
    header.config_timestamp = std::time(nullptr);
    header.user_table_offset = sizeof(OMNIHeader);
    header.max_users = 1024;
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));


    UserInfo empty_user;
    for (uint32_t i = 0; i < header.max_users; ++i)
        ofs.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));


    FileEntry root_entry("root", EntryType::DIRECTORY, 0, 0755, "admin", 0);
    ofs.write(reinterpret_cast<const char*>(&root_entry), sizeof(FileEntry));


    uint64_t total_blocks = header.total_size / header.block_size;
    FreeSpaceManager fsm(total_blocks);
    uint64_t used_blocks = (sizeof(OMNIHeader) + header.max_users * sizeof(UserInfo) + sizeof(FileEntry) + header.block_size - 1) / header.block_size;
    for (uint64_t i = 0; i < used_blocks; ++i) fsm.markUsed(i);

    ofs.seekp(0, std::ios::end);
    const std::vector<uint8_t>& bitmap = fsm.getBitmap();
    ofs.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());

    ofs.close();
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int fs_init(void** instance, const char* omni_path, const char* config_path) {
    std::ifstream ifs(omni_path, std::ios::binary);
    if (!ifs.is_open()) return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);

    FSInstance* fs = new FSInstance();
    fs->omni_path = omni_path;

    
    ifs.read(reinterpret_cast<char*>(&fs->header), sizeof(OMNIHeader));


    fs->users = new HashTable<UserInfo>(fs->header.max_users);
    ifs.seekg(fs->header.user_table_offset, std::ios::beg);
    for (uint32_t i = 0; i < fs->header.max_users; ++i) {
        UserInfo u;
        ifs.read(reinterpret_cast<char*>(&u), sizeof(UserInfo));
        if (u.is_active) fs->users->insert(u.username, new UserInfo(u));
    }
    uint64_t fs_tree_start = fs->header.user_table_offset + fs->header.max_users * sizeof(UserInfo);
    ifs.seekg(0, std::ios::end);
    uint64_t fs_end = ifs.tellg();
    uint64_t bitmap_size = (fs->header.total_size / fs->header.block_size + 7) / 8;
    uint64_t fs_tree_end = fs_end - bitmap_size;
    uint64_t offset = fs_tree_start;
    fs->root = load_fs_tree(ifs, offset, fs_tree_end);

    
    uint64_t total_blocks = fs->header.total_size / fs->header.block_size;
    fs->fsm = new FreeSpaceManager(total_blocks);
    ifs.seekg(fs_tree_end, std::ios::beg);
    std::vector<uint8_t> bitmap(bitmap_size);
    ifs.read(reinterpret_cast<char*>(bitmap.data()), bitmap_size);
    fs->fsm->setBitmap(bitmap);

    *instance = fs;
    ifs.close();
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}


void fs_shutdown(void* instance) {
    if (!instance) return;
    FSInstance* fs = static_cast<FSInstance*>(instance);

    std::ofstream ofs(fs->omni_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "Error: cannot write FS to disk during shutdown\n";
        return;
    }

    
    ofs.write(reinterpret_cast<const char*>(&fs->header), sizeof(OMNIHeader));

    
    for (uint32_t i = 0; i < fs->header.max_users; ++i) {
        UserInfo* user = fs->users->get(fs->users->getKeyAt(i)); 
        if (user) ofs.write(reinterpret_cast<const char*>(user), sizeof(UserInfo));
        else {
            UserInfo empty;
            ofs.write(reinterpret_cast<const char*>(&empty), sizeof(UserInfo));
        }
    }


    serialize_fs_tree(fs->root, ofs);

    
    const std::vector<uint8_t>& bitmap = fs->fsm->getBitmap();
    ofs.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());

    ofs.close();


    delete fs->fsm;
    delete fs->root;
    delete fs->users;
    for (auto session : fs->sessions) delete session;
    delete fs;
}
