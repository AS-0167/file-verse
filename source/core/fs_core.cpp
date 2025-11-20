#include "fs_core.h"
#include <fstream>
#include <cstring>
#include <ctime>
#include <iostream>
#include "core/user_manager.h"
#include <cstring>
#include <vector>
#include <openssl/sha.h>
#include <cstdio>  // for sprintf
#include <algorithm>
#include "FSNode.h"

struct FSConfig {
    uint64_t total_size = 0;
    uint32_t header_size = 0;
    uint32_t block_size = 0;

    uint32_t max_users = 0;
    char admin_username[64] = {0};
    char admin_password[128] = {0};
};

bool load_config(const char* path, FSConfig &cfg) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    std::string section;

    auto trim = [](std::string &s) {
        while (!s.empty() && std::isspace(s.front())) s.erase(0,1);
        while (!s.empty() && std::isspace(s.back())) s.pop_back();
    };

    while (std::getline(file, line)) {
        // Trim right side only first to remove '\n' or '\r'
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) 
            line.pop_back();

        // Remove inline comments
        size_t hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);

        trim(line); // Trim spaces both sides now

        if (line.empty()) continue;

        // Section header
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            trim(section);
            continue;
        }

        // Key=Value parsing
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key   = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        trim(key);
        trim(value);

        // Remove surrounding quotes if present
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        // Assign values
        if (section == "filesystem") {
            if (key == "total_size")      cfg.total_size  = std::stoull(value);
            else if (key == "header_size") cfg.header_size = std::stoul(value);
            else if (key == "block_size")  cfg.block_size  = std::stoul(value);
        } else if (section == "security") {
            if (key == "max_users")       cfg.max_users = std::stoul(value);
            else if (key == "admin_username")
                strncpy(cfg.admin_username, value.c_str(), sizeof(cfg.admin_username)-1);
            else if (key == "admin_password")
                strncpy(cfg.admin_password, value.c_str(), sizeof(cfg.admin_password)-1);
        }
    }

    return true;
}

void shift_encrypt(char* buf, size_t size, int shift) {
    for (size_t i = 0; i < size; ++i) {
        buf[i] = buf[i] + shift; // +1 for writing
    }
}

void shift_decrypt(char* buf, size_t size, int shift) {
    for (size_t i = 0; i < size; ++i) {
        buf[i] = buf[i] - shift; // -1 for reading
    }
}
// --- serializer ---
void write_all_encrypted(std::ofstream &ofs, char *buf, std::streamsize n, int shift = 1) {
    std::vector<char> temp(buf, buf + n);  // make a copy
    shift_encrypt(temp.data(), n, shift);  // encrypt
    ofs.write(temp.data(), n);             // write encrypted
}

void read_all_decrypted(std::ifstream &ifs, char *buf, std::streamsize n, int shift = 1) {
    ifs.read(buf, n);
    std::streamsize read_bytes = ifs.gcount();
    if (read_bytes > 0) {
        shift_decrypt(buf, static_cast<size_t>(read_bytes), shift);
    }
}

int serialize_fs_tree(FSNode* node, std::ofstream& ofs, int shift = 1) {
    if (!node || !node->entry) return 0;

    // Encrypt and write FileEntry
    write_all_encrypted(ofs, reinterpret_cast<char*>(node->entry), sizeof(FileEntry), shift);

    if (node->entry->getType() == EntryType::FILE) {
        uint64_t data_len = static_cast<uint64_t>(node->data.size());
        write_all_encrypted(ofs, reinterpret_cast<char*>(&data_len), sizeof(uint64_t), shift);
        if (data_len > 0)
            write_all_encrypted(ofs, node->data.data(), static_cast<std::streamsize>(data_len), shift);

        uint32_t zero_children = 0;
        write_all_encrypted(ofs, reinterpret_cast<char*>(&zero_children), sizeof(uint32_t), shift);

    } else { // directory
        uint32_t count = 0;
        if (node->children) {
            auto child_node = node->children->getHead();
            while (child_node) {
                ++count;
                child_node = child_node->next;
            }
        }
        write_all_encrypted(ofs, reinterpret_cast<char*>(&count), sizeof(uint32_t), shift);

        if (count > 0) {
            auto child_node = node->children->getHead();
            while (child_node) {
                serialize_fs_tree(child_node->data, ofs, shift);
                child_node = child_node->next;
            }
        }
    }

    return 0;
}

FSNode* load_fs_tree(std::ifstream& ifs, uint64_t& offset, uint64_t end_offset, int shift = 1) {
    if (offset + sizeof(FileEntry) > end_offset) return nullptr;

    ifs.seekg(offset, std::ios::beg);

    FileEntry entry;
    read_all_decrypted(ifs, reinterpret_cast<char*>(&entry), sizeof(FileEntry), shift);
    offset += sizeof(FileEntry);

    FSNode* node = new FSNode(new FileEntry(entry));

    if (entry.getType() == EntryType::FILE) {
        if (offset + sizeof(uint64_t) > end_offset) return node;

        uint64_t data_len = 0;
        read_all_decrypted(ifs, reinterpret_cast<char*>(&data_len), sizeof(uint64_t), shift);
        offset += sizeof(uint64_t);

        if (data_len > 0) {
            if (offset + data_len > end_offset) {
                uint64_t available = end_offset - offset;
                node->data.resize(static_cast<size_t>(available));
                read_all_decrypted(ifs, node->data.data(), static_cast<std::streamsize>(available), shift);
                offset += available;
                return node;
            }
            node->data.resize(static_cast<size_t>(data_len));
            read_all_decrypted(ifs, node->data.data(), static_cast<std::streamsize>(data_len), shift);
            offset += data_len;
        }

        if (offset + sizeof(uint32_t) <= end_offset) {
            uint32_t maybe_children = 0;
            read_all_decrypted(ifs, reinterpret_cast<char*>(&maybe_children), sizeof(uint32_t), shift);
            offset += sizeof(uint32_t);
        }

    } else { // directory
        if (offset + sizeof(uint32_t) > end_offset) return node;

        uint32_t child_count = 0;
        read_all_decrypted(ifs, reinterpret_cast<char*>(&child_count), sizeof(uint32_t), shift);
        offset += sizeof(uint32_t);

        for (uint32_t i = 0; i < child_count; ++i) {
            FSNode* child = load_fs_tree(ifs, offset, end_offset, shift);
            if (!child) break;
            node->addChild(child);
        }
    }

    return node;
}


//The Last workinh one
/*
void write_all(std::ofstream &ofs, const char *buf, std::streamsize n) {
    ofs.write(buf, n);
}

int serialize_fs_tree(FSNode* node, std::ofstream& ofs) {
    if (!node || !node->entry) return 0;

    // 1) write FileEntry (fixed-size POD, safe)
    write_all(ofs, reinterpret_cast<const char*>(node->entry), sizeof(FileEntry));

    // 2) if file -> write data length (uint64_t) and data bytes
    if (node->entry->getType() == EntryType::FILE) {
        uint64_t data_len = static_cast<uint64_t>(node->data.size());
        ofs.write(reinterpret_cast<const char*>(&data_len), sizeof(uint64_t));
        if (data_len > 0)
            write_all(ofs, node->data.data(), static_cast<std::streamsize>(data_len));
        // For files, still write child count zero for forward compatibility (optional)
        uint32_t zero_children = 0;
        ofs.write(reinterpret_cast<const char*>(&zero_children), sizeof(uint32_t));
    } else {
        // 3) directory: count children and write count then recurse
        uint32_t count = 0;
        if (node->children) {
            auto child_node = node->children->getHead();
            while (child_node) {
                ++count;
                child_node = child_node->next;
            }
        }
        ofs.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));

        if (count > 0) {
            auto child_node = node->children->getHead();
            while (child_node) {
                serialize_fs_tree(child_node->data, ofs);
                child_node = child_node->next;
            }
        }
    }

    return 0;
}

// --- deserializer ---
FSNode* load_fs_tree(std::ifstream& ifs, uint64_t& offset, uint64_t end_offset) {
    // Make sure there's room for a FileEntry
    if (offset + sizeof(FileEntry) > end_offset) return nullptr;

    ifs.seekg(offset, std::ios::beg);

    FileEntry entry;
    ifs.read(reinterpret_cast<char*>(&entry), sizeof(FileEntry));
    if (ifs.gcount() != sizeof(FileEntry)) return nullptr;
    offset += sizeof(FileEntry);

    // create node
    FSNode* node = new FSNode(new FileEntry(entry));

    if (entry.getType() == (EntryType::FILE)) {
        // read data length
        if (offset + sizeof(uint64_t) > end_offset) {
            // corrupted: no length
            return node;
        }
        uint64_t data_len = 0;
        ifs.read(reinterpret_cast<char*>(&data_len), sizeof(uint64_t));
        if (ifs.gcount() != sizeof(uint64_t)) return node;
        offset += sizeof(uint64_t);

        if (data_len > 0) {
            if (offset + data_len > end_offset) {
                // corrupted: declared size exceeds available
                // clamp to available bytes or return node with empty data
                uint64_t available = end_offset - offset;
                node->data.resize(static_cast<size_t>(available));
                ifs.read(reinterpret_cast<char*>(node->data.data()), static_cast<std::streamsize>(available));
                offset += available;
                return node;
            }
            node->data.resize(static_cast<size_t>(data_len));
            ifs.read(reinterpret_cast<char*>(node->data.data()), static_cast<std::streamsize>(data_len));
            if (static_cast<uint64_t>(ifs.gcount()) != data_len) {
                // partial read => leave as-is
            }
            offset += data_len;
        }

        // read and discard child count if writer wrote it (we wrote zero)
        if (offset + sizeof(uint32_t) <= end_offset) {
            uint32_t maybe_children = 0;
            ifs.read(reinterpret_cast<char*>(&maybe_children), sizeof(uint32_t));
            if (ifs.gcount() == sizeof(uint32_t)) offset += sizeof(uint32_t);
            else {
                // nothing
            }
        }

    } else { // directory
        // read child count
        if (offset + sizeof(uint32_t) > end_offset) return node;
        uint32_t child_count = 0;
        ifs.read(reinterpret_cast<char*>(&child_count), sizeof(uint32_t));
        if (ifs.gcount() != sizeof(uint32_t)) return node;
        offset += sizeof(uint32_t);

        for (uint32_t i = 0; i < child_count; ++i) {
            FSNode* child = load_fs_tree(ifs, offset, end_offset);
            if (!child) break;
            node->addChild(child);
        }
    }

    return node;
}
*/

/*
int serialize_fs_tree(FSNode* node, std::ofstream& ofs) {
    if (!node || !node->entry) return 0;

    
    ofs.write(reinterpret_cast<const char*>(node->entry), sizeof(FileEntry));


    uint32_t count = 0;
    if (node->entry->getType() == EntryType::DIRECTORY && node->children) {
        auto child_node = node->children->getHead();
        while (child_node) {
            ++count;
            child_node = child_node->next;
        }
    }
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));


    if (count > 0) {
        auto child_node = node->children->getHead();
        while (child_node) {
            serialize_fs_tree(child_node->data, ofs);
            child_node = child_node->next;
        }
    }

    return 0;
}
*/
//PREVIOUS
/*
int serialize_fs_tree(FSNode* node, std::ofstream& ofs) {
    if (!node || !node->entry) return 0;

    // Write the current entry
    ofs.write(reinterpret_cast<const char*>(node->entry), sizeof(FileEntry));

    // Count children
    uint32_t count = 0;
    if (node->entry->getType() == EntryType::DIRECTORY && node->children) {
        auto child_node = node->children->getHead();
        while (child_node) {
            ++count;
            child_node = child_node->next;
        }
    }
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));

    // Recursively serialize children
    if (count > 0) {
        auto child_node = node->children->getHead();
        while (child_node) {
            serialize_fs_tree(child_node->data, ofs);
            child_node = child_node->next;
        }
    }

    return 0;
}
*/

/*
FSNode* load_fs_tree(std::ifstream& ifs, uint64_t& offset, uint64_t end_offset) {
    if (offset + sizeof(FileEntry) > end_offset) return nullptr;

    FileEntry entry;
    ifs.seekg(offset, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(&entry), sizeof(FileEntry));
    offset += sizeof(FileEntry);

    FSNode* node = new FSNode(new FileEntry(entry));

    if (entry.getType() == EntryType::DIRECTORY) {
        if (offset + sizeof(uint32_t) > end_offset) return node;
        uint32_t child_count;
        ifs.read(reinterpret_cast<char*>(&child_count), sizeof(uint32_t));
        offset += sizeof(uint32_t);

        for (uint32_t i = 0; i < child_count; ++i) {
            FSNode* child = load_fs_tree(ifs, offset, end_offset);
            if (!child) break;
            node->addChild(child);
        }
    }

    return node;
}*/

//Previous
/*
FSNode* load_fs_tree(std::ifstream& ifs, uint64_t& offset, uint64_t end_offset) {
    if (offset + sizeof(FileEntry) > end_offset) return nullptr;

    FileEntry entry;
    ifs.seekg(offset, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(&entry), sizeof(FileEntry));
    if (ifs.gcount() != sizeof(FileEntry)) return nullptr;
    offset += sizeof(FileEntry);

    FSNode* node = new FSNode(new FileEntry(entry));

    if (entry.getType() == EntryType::DIRECTORY) {
        if (offset + sizeof(uint32_t) > end_offset) return node;

        uint32_t child_count;
        ifs.read(reinterpret_cast<char*>(&child_count), sizeof(uint32_t));
        if (ifs.gcount() != sizeof(uint32_t)) return node;
        offset += sizeof(uint32_t);

        for (uint32_t i = 0; i < child_count; ++i) {
            FSNode* child = load_fs_tree(ifs, offset, end_offset);
            if (!child) break; // stop if corrupted
            node->addChild(child);
        }
    }

    return node;
}
*/



//----------------- SHA256 helper -----------------
std::string sha256(const std::string &password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), password.size(), hash);

    char buf[64];
    for (int i = 0; i < 31; i++)
        sprintf(buf + i*2, "%02x", hash[i]);
    buf[62] = 0;
    return std::string(buf);
}

// ----------------- fs_format -----------------
/*
int fs_format(const char* omni_path, const char* config_path) {
    std::ofstream ofs(omni_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);

    // Create OMNIHeader
    OMNIHeader header(0x00010000, 1024ULL * 1024 * 50, sizeof(OMNIHeader), 4096);
    std::strncpy(header.magic, "OMNIFS01", sizeof(header.magic) - 1);
    header.config_timestamp = std::time(nullptr);
    header.user_table_offset = sizeof(OMNIHeader);
    header.max_users = 1024;
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // ----------------- Default Admin -----------------
    std::string hashed_admin = sha256("admin123");
    UserInfo admin_user("admin", hashed_admin, UserRole::ADMIN, std::time(nullptr));
    ofs.write(reinterpret_cast<const char*>(&admin_user), sizeof(UserInfo));

    // ----------------- Empty Users -----------------
    UserInfo empty_user;
    std::memset(&empty_user, 0, sizeof(UserInfo));
    for (uint32_t i = 1; i < header.max_users; ++i)
        ofs.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));

    // ----------------- Root Directory -----------------
    FileEntry root_entry("root", EntryType::DIRECTORY, 0, 0755, "admin", 0);
    ofs.write(reinterpret_cast<const char*>(&root_entry), sizeof(FileEntry));

    // ----------------- Free Space Bitmap -----------------
    uint64_t total_blocks = header.total_size / header.block_size;
    FreeSpaceManager fsm(total_blocks);

    uint64_t used_blocks = (sizeof(OMNIHeader) + header.max_users * sizeof(UserInfo) + sizeof(FileEntry) + header.block_size - 1) / header.block_size;
    for (uint64_t i = 0; i < used_blocks; ++i)
        fsm.markUsed(i);

    const std::vector<uint8_t>& bitmap = fsm.getBitmap();
    ofs.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());

    ofs.close();
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}
*/
int fs_format(const char* omni_path, const char* config_path) {
    FSConfig cfg;
    if (!load_config(config_path, cfg))
    {
        cout <<"Error in Loading config";
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    std::ofstream ofs(omni_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    cout << cfg.total_size<<" "<<cfg.header_size<<" "<<cfg.header_size<<" "<<cfg.max_users<<" "<<cfg.admin_username<<cfg.admin_password;
    // ---------------- Header from config ----------------
    OMNIHeader header(0x00010000, cfg.total_size, cfg.header_size, cfg.block_size);

    std::strncpy(header.magic, "OMNIFS01", sizeof(header.magic)-1);
    header.config_timestamp = std::time(nullptr);
    header.user_table_offset = sizeof(OMNIHeader);
    header.max_users = cfg.max_users;

    ofs.write((char*)&header, sizeof(header));

    // ---------------- Admin from config -----------------
    std::string hashed = sha256(cfg.admin_password);
    UserInfo admin(cfg.admin_username, hashed, UserRole::ADMIN, std::time(nullptr));

    ofs.write((char*)&admin, sizeof(UserInfo));

    // ---------------- Empty user slots ------------------
    UserInfo empty_user{};
    for (uint32_t i=1; i<cfg.max_users; i++)
        ofs.write((char*)&empty_user, sizeof(UserInfo));

    // ---------------- Root directory --------------------
    FSNode root(new FileEntry("root", EntryType::DIRECTORY, 0, 0755, cfg.admin_username, 0));
    serialize_fs_tree(&root, ofs, 1);

    // ---------------- Bitmap ----------------------------
    uint64_t total_blocks = cfg.total_size / cfg.block_size;
    FreeSpaceManager fsm(total_blocks);

    uint64_t used_blocks =
        (sizeof(OMNIHeader) + cfg.max_users * sizeof(UserInfo)
         + sizeof(FileEntry) + cfg.block_size - 1) / cfg.block_size;

    for (uint64_t i=0; i<used_blocks; i++)
        fsm.markUsed(i);

    auto &bitmap = fsm.getBitmap();
    ofs.write((char*)bitmap.data(), bitmap.size());

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int fs_init(void** instance, const char* omni_path, const char* config_path) {
    std::ifstream ifs(omni_path, std::ios::binary);
    if (!ifs.is_open()) return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);

    FSInstance* fs = new FSInstance();
    fs->omni_path = omni_path;

    // --- Load header (unencrypted) ---
    ifs.read(reinterpret_cast<char*>(&fs->header), sizeof(OMNIHeader));

    // --- Load users (unencrypted) ---
    fs->users = new HashTable<UserInfo>(fs->header.max_users);
    ifs.seekg(fs->header.user_table_offset, std::ios::beg);
    for (uint32_t i = 0; i < fs->header.max_users; ++i) {
        UserInfo u;
        ifs.read(reinterpret_cast<char*>(&u), sizeof(UserInfo));
        if (u.is_active && u.username[0] != '\0' && strlen(u.username) > 0) {
            fs->users->insert(u.username, new UserInfo(u));
        }
    }

    // --- Load FS tree (encrypted) ---
    uint64_t fs_tree_start = fs->header.user_table_offset + fs->header.max_users * sizeof(UserInfo);
    ifs.seekg(0, std::ios::end);
    uint64_t fs_end = ifs.tellg();

    uint64_t bitmap_size = (fs->header.total_size / fs->header.block_size + 7) / 8;
    uint64_t fs_tree_end = fs_end - bitmap_size;

    uint64_t offset = fs_tree_start;
    fs->root = load_fs_tree(ifs, offset, fs_tree_end, 1);  // shift=1, decrypt

    // --- Load free-space bitmap (unencrypted) ---
    fs->fsm = new FreeSpaceManager(fs->header.total_size / fs->header.block_size);
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

    // --- Write header (unencrypted) ---
    ofs.write(reinterpret_cast<const char*>(&fs->header), sizeof(OMNIHeader));

    // --- Write users (unencrypted) ---
    uint32_t users_written = 0;
    for (auto head : fs->users->getBuckets()) {
        auto node = head;
        while (node) {
            ofs.write(reinterpret_cast<const char*>(node->value), sizeof(UserInfo));
            ++users_written;
            node = node->next;
        }
    }

    UserInfo empty_user;
    std::memset(&empty_user, 0, sizeof(UserInfo));
    for (; users_written < fs->header.max_users; ++users_written)
        ofs.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));

    // --- Write FS tree (encrypted) ---
    serialize_fs_tree(fs->root, ofs, 1);  // shift=1, encrypted

    // --- Write free-space bitmap (unencrypted) ---
    const std::vector<uint8_t>& bitmap = fs->fsm->getBitmap();
    ofs.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());

    ofs.close();

    // --- Cleanup memory ---
    
    delete fs->fsm;
    delete fs->root;
    delete fs->users;
    fs->sessions.clear();

    delete fs;
    
}

/*
int fs_init(void** instance, const char* omni_path, const char* config_path) {
    
    std::ifstream ifs(omni_path, std::ios::binary);
    if (!ifs.is_open()) return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    
    FSInstance* fs = new FSInstance();
    fs->omni_path = omni_path;

    // Load header
    ifs.read(reinterpret_cast<char*>(&fs->header), sizeof(OMNIHeader));

    // Load users
    fs->users = new HashTable<UserInfo>(fs->header.max_users);
    ifs.seekg(fs->header.user_table_offset, std::ios::beg);
    for (uint32_t i = 0; i < fs->header.max_users; ++i) {
        UserInfo u;
        ifs.read(reinterpret_cast<char*>(&u), sizeof(UserInfo));
        if (u.is_active && u.username[0] != '\0' && strlen(u.username) > 0) {
            fs->users->insert(u.username, new UserInfo(u));
        }
    }

    // Load FS tree
    uint64_t fs_tree_start = fs->header.user_table_offset + fs->header.max_users * sizeof(UserInfo);
    ifs.seekg(0, std::ios::end);
    uint64_t fs_end = ifs.tellg();
    uint64_t bitmap_size = (fs->header.total_size / fs->header.block_size + 7) / 8;
    uint64_t fs_tree_end = fs_end - bitmap_size;
    uint64_t offset = fs_tree_start;
    fs->root = load_fs_tree(ifs, offset, fs_tree_end); //Previous version
    //fs->root = load_fs_tree(ifs, offset);
    

    // Load bitmap
    fs->fsm = new FreeSpaceManager(fs->header.total_size / fs->header.block_size);
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


    uint32_t users_written = 0;
    for (auto head : fs->users->getBuckets()) {
        auto node = head;
        while (node) {
            ofs.write(reinterpret_cast<const char*>(node->value), sizeof(UserInfo));
            ++users_written;
            node = node->next;
        }
    }

    UserInfo empty_user;
    std::memset(&empty_user, 0, sizeof(UserInfo));
    for (; users_written < fs->header.max_users; ++users_written)
        ofs.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));


    serialize_fs_tree(fs->root, ofs);


    const std::vector<uint8_t>& bitmap = fs->fsm->getBitmap();
    ofs.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());

    ofs.close();

    delete fs->fsm;
    delete fs->root;
    delete fs->users;
    for (auto session : fs->sessions) 
      delete static_cast<SessionInfo*>(session);

    delete fs;
}





//Changed
/*
#include "fs_core.h"
#include <fstream>
#include <cstring>
#include <ctime>
#include <iostream>
#include "core/user_manager.h"
#include <cstring>
#include <vector>
#include <openssl/sha.h>
#include <cstdio>  // for sprintf
#include <algorithm>
#include <mutex>
#include <fstream>
#include <vector>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <mutex>
#include <sstream>


void shift_encrypt(char* buf, size_t size, int shift) {
    for (size_t i = 0; i < size; ++i) {
        buf[i] = buf[i] + shift; // +1 for writing
    }
}

void shift_decrypt(char* buf, size_t size, int shift) {
    for (size_t i = 0; i < size; ++i) {
        buf[i] = buf[i] - shift; // -1 for reading
    }
}


int serialize_fs_tree(FSNode* node, std::ofstream& ofs) {
    if (!node || !node->entry) return 0;

    
    ofs.write(reinterpret_cast<const char*>(node->entry), sizeof(FileEntry));


    uint32_t count = 0;
    if (node->entry->getType() == EntryType::DIRECTORY && node->children) {
        auto child_node = node->children->getHead();
        while (child_node) {
            ++count;
            child_node = child_node->next;
        }
    }
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));


    if (count > 0) {
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
        if (offset + sizeof(uint32_t) > end_offset) return node;
        uint32_t child_count;
        ifs.read(reinterpret_cast<char*>(&child_count), sizeof(uint32_t));
        offset += sizeof(uint32_t);

        for (uint32_t i = 0; i < child_count; ++i) {
            FSNode* child = load_fs_tree(ifs, offset, end_offset);
            if (!child) break;
            node->addChild(child);
        }
    }

    return node;
}




//----------------- SHA256 helper -----------------
std::string sha256(const std::string &password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), password.size(), hash);

    char buf[64];
    for (int i = 0; i < 31; i++)
        sprintf(buf + i*2, "%02x", hash[i]);
    buf[62] = 0;
    return std::string(buf);
}

// ----------------- fs_format -----------------
int fs_format(const char* omni_path, const char* config_path) {
    std::ofstream ofs(omni_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);

    // Create OMNIHeader
    OMNIHeader header(0x00010000, 1024ULL * 1024 * 50, sizeof(OMNIHeader), 4096);
    std::strncpy(header.magic, "OMNIFS01", sizeof(header.magic) - 1);
    header.config_timestamp = std::time(nullptr);
    header.user_table_offset = sizeof(OMNIHeader);
    header.max_users = 1024;
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // ----------------- Default Admin -----------------
    std::string hashed_admin = sha256("admin123");
    UserInfo admin_user("admin", hashed_admin, UserRole::ADMIN, std::time(nullptr));
    ofs.write(reinterpret_cast<const char*>(&admin_user), sizeof(UserInfo));

    // ----------------- Empty Users -----------------
    UserInfo empty_user;
    std::memset(&empty_user, 0, sizeof(UserInfo));
    for (uint32_t i = 1; i < header.max_users; ++i)
        ofs.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));

    // ----------------- Root Directory -----------------
    FileEntry root_entry("root", EntryType::DIRECTORY, 0, 0755, "admin", 0);
    ofs.write(reinterpret_cast<const char*>(&root_entry), sizeof(FileEntry));

    // ----------------- Free Space Bitmap -----------------
    uint64_t total_blocks = header.total_size / header.block_size;
    FreeSpaceManager fsm(total_blocks);

    uint64_t used_blocks = (sizeof(OMNIHeader) + header.max_users * sizeof(UserInfo) + sizeof(FileEntry) + header.block_size - 1) / header.block_size;
    for (uint64_t i = 0; i < used_blocks; ++i)
        fsm.markUsed(i);

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

    // Load header
    ifs.read(reinterpret_cast<char*>(&fs->header), sizeof(OMNIHeader));

    // Load users
    fs->users = new HashTable<UserInfo>(fs->header.max_users);
    ifs.seekg(fs->header.user_table_offset, std::ios::beg);
    for (uint32_t i = 0; i < fs->header.max_users; ++i) {
        UserInfo u;
        ifs.read(reinterpret_cast<char*>(&u), sizeof(UserInfo));
        if (u.is_active && u.username[0] != '\0' && strlen(u.username) > 0) {
            fs->users->insert(u.username, new UserInfo(u));
        }
    }

    // Load FS tree
    uint64_t fs_tree_start = fs->header.user_table_offset + fs->header.max_users * sizeof(UserInfo);
    ifs.seekg(0, std::ios::end);
    uint64_t fs_end = ifs.tellg();
    uint64_t bitmap_size = (fs->header.total_size / fs->header.block_size + 7) / 8;
    uint64_t fs_tree_end = fs_end - bitmap_size;
    uint64_t offset = fs_tree_start;
    fs->root = load_fs_tree(ifs, offset, fs_tree_end); //Previous version
    //fs->root = load_fs_tree(ifs, offset);
    

    // Load bitmap
    fs->fsm = new FreeSpaceManager(fs->header.total_size / fs->header.block_size);
    ifs.seekg(fs_tree_end, std::ios::beg);
    std::vector<uint8_t> bitmap(bitmap_size);
    ifs.read(reinterpret_cast<char*>(bitmap.data()), bitmap_size);
    fs->fsm->setBitmap(bitmap);

    *instance = fs;
    ifs.close();
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

   static std::mutex fs_global_mutex;

// Helper: serialize the FS tree into a memory buffer (so we know its size)
static void serialize_fs_tree_to_stream(FSNode* node, std::ostream &os) {
    if (!node || !node->entry) return;

    // write entry
    os.write(reinterpret_cast<const char*>(node->entry), sizeof(FileEntry));

    // count children if directory
    uint32_t count = 0;
    if (node->entry->getType() == EntryType::DIRECTORY && node->children) {
        auto child_node = node->children->getHead();
        while (child_node) {
            ++count;
            child_node = child_node->next;
        }
    }
    os.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));

    if (count > 0) {
        auto child_node = node->children->getHead();
        while (child_node) {
            serialize_fs_tree_to_stream(child_node->data, os);
            child_node = child_node->next;
        }
    }
}

// safe atomic shutdown that preserves blocks
void fs_shutdown(void* instance) {
    if (!instance) return;
    FSInstance* fs = static_cast<FSInstance*>(instance);

    // Lock the global mutex to prevent concurrent mutations while we snapshot/write
    std::lock_guard<std::mutex> guard(fs_global_mutex);

    string orig_path = fs->omni_path;
    string tmp_path = std::string(orig_path) + ".tmp";

    // Open original file for reading (to copy used blocks)
    std::ifstream ifs(orig_path, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Error: cannot open original omni file for reading during shutdown\n";
        // Still try to write a safe minimal image (fallback)
    }

    // Build serialized tree into memory buffer
    std::ostringstream tree_buf_oss(std::ios::binary);
    serialize_fs_tree_to_stream(fs->root, tree_buf_oss);
    std::string tree_buf = tree_buf_oss.str();
    size_t tree_size = tree_buf.size();

    // Prepare bitmap and sizes
    uint64_t total_blocks = fs->header.total_size / fs->header.block_size;
    uint64_t block_size = fs->header.block_size;
    size_t bitmap_size = (total_blocks + 7) / 8;

    // Create tmp file and pre-allocate final file size (header.total_size)
    std::ofstream ofs(tmp_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "Error: cannot create temporary omni file: " << tmp_path << "\n";
        if (ifs.is_open()) ifs.close();
        return;
    }

    // Pre-allocate: set file length to header.total_size (so block offsets are valid)
    // Seek to total_size - 1 and write a zero byte
    uint64_t target_size = fs->header.total_size;
    if (target_size == 0) {
        // fallback: if header.total_size invalid, compute a safe target using old file size if available
        if (ifs.is_open()) {
            ifs.seekg(0, std::ios::end);
            target_size = static_cast<uint64_t>(ifs.tellg());
        } else {
            target_size = block_size * std::max<uint64_t>(total_blocks, 1);
        }
    }
    ofs.seekp(target_size > 0 ? (std::streamoff)target_size - 1 : 0, std::ios::beg);
    char zero = 0;
    ofs.write(&zero, 1);
    ofs.flush();
    ofs.close();

    // Reopen tmp as fstream for random access write
    std::fstream tmpfs(tmp_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!tmpfs.is_open()) {
        std::cerr << "Error: cannot reopen temporary omni file for writing: " << tmp_path << "\n";
        if (ifs.is_open()) ifs.close();
        return;
    }

    // 1) Write header at beginning
    tmpfs.seekp(0, std::ios::beg);
    tmpfs.write(reinterpret_cast<const char*>(&fs->header), sizeof(OMNIHeader));

    // 2) Write users table at header.user_table_offset
    uint64_t user_table_offset = fs->header.user_table_offset;
    tmpfs.seekp(user_table_offset, std::ios::beg);

    // First write all active users from hashtable buckets
    uint32_t users_written = 0;
    for (auto head : fs->users->getBuckets()) {
        auto node = head;
        while (node) {
            tmpfs.write(reinterpret_cast<const char*>(node->value), sizeof(UserInfo));
            ++users_written;
            node = node->next;
        }
    }
    // Fill remaining slots with empty entries
    UserInfo empty_user;
    std::memset(&empty_user, 0, sizeof(UserInfo));
    for (; users_written < fs->header.max_users; ++users_written) {
        tmpfs.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));
    }

    // 3) Write serialized tree at the expected fs_tree_start
    uint64_t fs_tree_start = fs->header.user_table_offset + fs->header.max_users * sizeof(UserInfo);
    tmpfs.seekp(fs_tree_start, std::ios::beg);
    if (tree_size > 0) tmpfs.write(tree_buf.data(), (std::streamsize)tree_size);

    // 4) Copy data blocks from old file to correct offsets in tmp file.
    //    We use the in-memory bitmap (fs->fsm->getBitmap()) to decide which block indexes are used and copy them.
    std::vector<uint8_t> bitmap = fs->fsm->getBitmap();
    if (ifs.is_open()) {
        for (uint64_t block_index = 0; block_index < total_blocks; ++block_index) {
            uint64_t byte_index = block_index / 8;
            uint8_t bit_mask = (uint8_t)(1u << (block_index % 8));
            bool is_used = false;
            if (byte_index < bitmap.size()) {
                is_used = (bitmap[byte_index] & bit_mask) != 0;
            }
            if (!is_used) continue;

            // compute offset for this block in bytes
            uint64_t block_offset = block_index * block_size;

            // read block from original file
            std::vector<char> block_data(block_size);
            ifs.seekg((std::streamoff)block_offset, std::ios::beg);
            ifs.read(block_data.data(), (std::streamsize)block_size);
            std::streamsize r = ifs.gcount();
            if (r <= 0) continue; // nothing to copy (should not normally happen)

            // write block into tmp file at same offset
            tmpfs.seekp((std::streamoff)block_offset, std::ios::beg);
            tmpfs.write(block_data.data(), r);
        }
    } else {
        // No existing file to copy from: nothing to do (fresh format scenario)
    }

    // 5) Write the bitmap at the end-of-file position expected by your loader:
    //    loader computes bitmap_size = (total_blocks +7)/8 and treats it as being placed at the end.
    uint64_t bitmap_offset = target_size >= bitmap_size ? (target_size - bitmap_size) : 0;
    tmpfs.seekp((std::streamoff)bitmap_offset, std::ios::beg);
    tmpfs.write(reinterpret_cast<const char*>(bitmap.data()), (std::streamsize)bitmap_size);

    // flush and fsync tmp file (POSIX)
// flush and close the std::fstream first
tmpfs.flush();
tmpfs.close();

// get a POSIX file descriptor for the temp file and fsync it
int fd = open(tmp_path.c_str(), O_RDWR);
if (fd >= 0) {
    if (fsync(fd) != 0) {
        perror("fsync");
    }
    close(fd);
} else {
    perror("open tmp file for fsync");
}

    tmpfs.close();
    if (ifs.is_open()) ifs.close();

    // atomic rename
    if (std::rename(tmp_path.c_str(), orig_path.c_str()) != 0) {
        std::perror("rename");
        std::cerr << "Warning: could not atomically replace omni file. tmp file retained: " << tmp_path << "\n";
        // do not remove tmp file here so user can inspect
    }

    // cleanup in-memory structures
    delete fs->fsm;
    delete fs->root;
    delete fs->users;
    for (auto session : fs->sessions)
        delete static_cast<SessionInfo*>(session);
    delete fs;
}

*/


//Previously COmmented
/*
int serialize_fs_tree(FSNode* node, std::ofstream& ofs) {
    if (!node || !node->entry) return 0;

    // Encrypt and write the current entry
    FileEntry temp = *node->entry;
    shift_encrypt(reinterpret_cast<char*>(&temp), sizeof(FileEntry), 1);
    ofs.write(reinterpret_cast<const char*>(&temp), sizeof(FileEntry));

    // Count children
    uint32_t count = 0;
    if (node->entry->getType() == EntryType::DIRECTORY && node->children) {
        auto child_node = node->children->getHead();
        while (child_node) {
            ++count;
            child_node = child_node->next;
        }
    }
    
    // ✅ Encrypt child count before writing
    uint32_t encrypted_count = count;
    shift_encrypt(reinterpret_cast<char*>(&encrypted_count), sizeof(uint32_t), 1);
    ofs.write(reinterpret_cast<const char*>(&encrypted_count), sizeof(uint32_t));

    // Recursively serialize children
    if (count > 0) {
        auto child_node = node->children->getHead();
        while (child_node) {
            serialize_fs_tree(child_node->data, ofs);
            child_node = child_node->next;
        }
    }

    return 0;
}
*/

//Commented
/*
int serialize_fs_tree(FSNode* node, std::ofstream& ofs) {
    if (!node || !node->entry) return 0;

    // Write the current entry
    ofs.write(reinterpret_cast<const char*>(node->entry), sizeof(FileEntry));

    // Count children
    uint32_t count = 0;
    if (node->entry->getType() == EntryType::DIRECTORY && node->children) {
        auto child_node = node->children->getHead();
        while (child_node) {
            ++count;
            child_node = child_node->next;
        }
    }
    ofs.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));

    // Recursively serialize children
    if (count > 0) {
        auto child_node = node->children->getHead();
        while (child_node) {
            serialize_fs_tree(child_node->data, ofs);
            child_node = child_node->next;
        }
    }

    return 0;
}
*/
/*
void fs_shutdown(void* instance) {
    if (!instance) return;
    FSInstance* fs = static_cast<FSInstance*>(instance);

    std::ofstream ofs(fs->omni_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "Error: cannot write FS to disk during shutdown\n";
        return;
    }

    
    ofs.write(reinterpret_cast<const char*>(&fs->header), sizeof(OMNIHeader));


    uint32_t users_written = 0;
    for (auto head : fs->users->getBuckets()) {
        auto node = head;
        while (node) {
            ofs.write(reinterpret_cast<const char*>(node->value), sizeof(UserInfo));
            ++users_written;
            node = node->next;
        }
    }

    UserInfo empty_user;
    std::memset(&empty_user, 0, sizeof(UserInfo));
    for (; users_written < fs->header.max_users; ++users_written)
        ofs.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));


    serialize_fs_tree(fs->root, ofs);


    const std::vector<uint8_t>& bitmap = fs->fsm->getBitmap();
    ofs.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());

    ofs.close();

    delete fs->fsm;
    delete fs->root;
    delete fs->users;
    for (auto session : fs->sessions) 
      delete static_cast<SessionInfo*>(session);

    delete fs;
}
    */
 

//Commented 
/*
FSNode* load_fs_tree(std::ifstream& ifs, uint64_t& offset, uint64_t end_offset) {
    if (offset + sizeof(FileEntry) > end_offset) return nullptr;

    FileEntry entry;
    ifs.seekg(offset, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(&entry), sizeof(FileEntry));
    shift_decrypt(reinterpret_cast<char*>(&entry), sizeof(FileEntry), 1);
    
    if (ifs.gcount() != sizeof(FileEntry)) return nullptr;
    offset += sizeof(FileEntry);

    FSNode* node = new FSNode(new FileEntry(entry));

    if (entry.getType() == EntryType::DIRECTORY) {
        if (offset + sizeof(uint32_t) > end_offset) return node;

        uint32_t child_count;
        ifs.read(reinterpret_cast<char*>(&child_count), sizeof(uint32_t));
        
        // ✅ Decrypt child count after reading
        shift_decrypt(reinterpret_cast<char*>(&child_count), sizeof(uint32_t), 1);
        
        if (ifs.gcount() != sizeof(uint32_t)) return node;
        offset += sizeof(uint32_t);

        for (uint32_t i = 0; i < child_count; ++i) {
            FSNode* child = load_fs_tree(ifs, offset, end_offset);
            if (!child) break;
            node->addChild(child);
        }
    }

    return node;
}

*/

/*
void fs_shutdown(void* instance) {
    if (!instance) return;
    FSInstance* fs = static_cast<FSInstance*>(instance);

    std::ofstream ofs(fs->omni_path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        std::cerr << "Error: cannot write FS to disk during shutdown\n";
        return;
    }

    
    ofs.write(reinterpret_cast<const char*>(&fs->header), sizeof(OMNIHeader));


    uint32_t users_written = 0;
    for (auto head : fs->users->getBuckets()) {
        auto node = head;
        while (node) {
            ofs.write(reinterpret_cast<const char*>(node->value), sizeof(UserInfo));
            ++users_written;
            node = node->next;
        }
    }

    UserInfo empty_user;
    std::memset(&empty_user, 0, sizeof(UserInfo));
    for (; users_written < fs->header.max_users; ++users_written)
        ofs.write(reinterpret_cast<const char*>(&empty_user), sizeof(UserInfo));


    serialize_fs_tree(fs->root, ofs);


    const std::vector<uint8_t>& bitmap = fs->fsm->getBitmap();
    ofs.write(reinterpret_cast<const char*>(bitmap.data()), bitmap.size());

    ofs.close();

    delete fs->fsm;
    delete fs->root;
    delete fs->users;
    for (auto session : fs->sessions) 
      delete static_cast<SessionInfo*>(session);

    delete fs;
}
    */