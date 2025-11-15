// In file: source/core/filesystem.cpp

#include <fstream>
#include <vector>
#include <string>
#include <ctime>
#include <cstring>
#include <sstream>
#include <algorithm>

#include "include/filesystem.h"
#include "include/ofs_types.h"
#include "include/hash_table.h"

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
int find_entry_by_path(FileSystemInstance& fs_instance, const std::string& path);
int find_free_metadata_entry(FileSystemInstance& fs_instance);
int find_free_block(FileSystemInstance& fs_instance);

// ============================================================================
// CORE SYSTEM FUNCTIONS
// ============================================================================
void fs_format(const std::string& file_path) {
    const uint64_t TOTAL_FS_SIZE = 100 * 1024 * 1024;
    const uint64_t BLOCK_SIZE = 4096;
    const uint32_t METADATA_COUNT = 1000;
    const uint32_t MAX_USERS = 50;

    OMNIHeader header = {};
    memcpy(header.magic, "OMNIFS01", sizeof(header.magic));
    header.format_version = 0x00010000;
    header.total_size = TOTAL_FS_SIZE;
    header.header_size = sizeof(OMNIHeader);
    header.block_size = BLOCK_SIZE;
    header.max_users = MAX_USERS;
    header.user_table_offset = sizeof(OMNIHeader);

    std::vector<UserInfo> user_table(MAX_USERS, UserInfo{});
    UserInfo& admin_user = user_table[0];
    strncpy(admin_user.username, "admin", sizeof(admin_user.username) - 1);
    strncpy(admin_user.password_hash, "admin123", sizeof(admin_user.password_hash) - 1);
    admin_user.role = 1;
    admin_user.is_active = 1;
    admin_user.created_time = time(nullptr);

    std::vector<MetadataEntry> metadata_table(METADATA_COUNT, MetadataEntry{});
    for(size_t i = 0; i < METADATA_COUNT; ++i) {
        metadata_table[i].validity_flag = 1;
    }
    MetadataEntry& root_dir = metadata_table[0];
    root_dir.validity_flag = 0;
    root_dir.type_flag = 1;
    root_dir.parent_index = 0;
    strncpy(root_dir.short_name, "/", sizeof(root_dir.short_name) - 1);
    root_dir.owner_id = 0;
    root_dir.created_time = time(nullptr);
    root_dir.modified_time = time(nullptr);

    std::ofstream ofs(file_path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(OMNIHeader));
    ofs.write(reinterpret_cast<const char*>(user_table.data()), MAX_USERS * sizeof(UserInfo));
    ofs.write(reinterpret_cast<const char*>(metadata_table.data()), METADATA_COUNT * sizeof(MetadataEntry));
    
    uint64_t current_size = ofs.tellp();
    if (TOTAL_FS_SIZE > current_size) {
        std::vector<char> empty_space(TOTAL_FS_SIZE - current_size, 0);
        ofs.write(empty_space.data(), empty_space.size());
    }
    ofs.close();
}

void fs_init(FileSystemInstance& fs_instance, const std::string& file_path) {
    fs_instance.omni_file_path = file_path;
    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs) return;

    ifs.read(reinterpret_cast<char*>(&fs_instance.header), sizeof(OMNIHeader));
    fs_instance.user_table.resize(fs_instance.header.max_users);
    ifs.read(reinterpret_cast<char*>(fs_instance.user_table.data()), fs_instance.header.max_users * sizeof(UserInfo));
    
    fs_instance.user_hash_table = hash_table_create(fs_instance.header.max_users * 2);
    for (size_t i = 0; i < fs_instance.user_table.size(); ++i) {
        if (fs_instance.user_table[i].is_active == 1) {
            hash_table_insert(fs_instance.user_hash_table, fs_instance.user_table[i].username, &fs_instance.user_table[i]);
        }
    }
    
    const uint32_t METADATA_COUNT = 1000;
    fs_instance.metadata_entries.resize(METADATA_COUNT);
    ifs.read(reinterpret_cast<char*>(fs_instance.metadata_entries.data()), METADATA_COUNT * sizeof(MetadataEntry));
    
    long data_area_start = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (METADATA_COUNT * sizeof(MetadataEntry));
    uint64_t total_data_blocks = (fs_instance.header.total_size - data_area_start) / fs_instance.header.block_size;
    fs_instance.free_block_map.assign(total_data_blocks, true);
    for(const auto& entry : fs_instance.metadata_entries) {
        if (entry.validity_flag == 0 && entry.start_index > 0) {
            if (entry.start_index < fs_instance.free_block_map.size()) {
                fs_instance.free_block_map[entry.start_index] = false;
            }
        }
    }
    ifs.close();
}

// ============================================================================
// ALL FUNCTIONS BELOW ARE NOW SILENT (NO std::cout or std::cerr)
// ============================================================================

bool user_login(FileSystemInstance& fs_instance, const std::string& username, const std::string& password) {
    UserInfo* user = hash_table_get(fs_instance.user_hash_table, username);
    if (user != nullptr && user->is_active == 1) { 
        return (strcmp(user->password_hash, password.c_str()) == 0);
    }
    return false;
}

void user_create(FileSystemInstance& fs_instance, const std::string& username, const std::string& password, uint32_t role) {
    int free_user_slot = -1;
    for (size_t i = 0; i < fs_instance.user_table.size(); ++i) {
        if (fs_instance.user_table[i].is_active == 0) {
            free_user_slot = i; break;
        }
        if (strcmp(fs_instance.user_table[i].username, username.c_str()) == 0) return;
    }
    if (free_user_slot == -1) return;

    UserInfo& new_user = fs_instance.user_table[free_user_slot];
    new_user.is_active = 1; new_user.role = role;
    strncpy(new_user.username, username.c_str(), sizeof(new_user.username) - 1);
    strncpy(new_user.password_hash, password.c_str(), sizeof(new_user.password_hash) - 1);
    new_user.created_time = time(nullptr);
    hash_table_insert(fs_instance.user_hash_table, new_user.username, &new_user);

    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(fs_instance.header.user_table_offset);
    file.write(reinterpret_cast<const char*>(fs_instance.user_table.data()), fs_instance.header.max_users * sizeof(UserInfo));
    file.close();
}

void user_delete(FileSystemInstance& fs_instance, const std::string& username) {
    if (username == "admin") return;
    int user_slot = -1;
    for (size_t i = 0; i < fs_instance.user_table.size(); ++i) {
        if (fs_instance.user_table[i].is_active == 1 && strcmp(fs_instance.user_table[i].username, username.c_str()) == 0) {
            user_slot = i; break;
        }
    }
    if (user_slot == -1) return;
    fs_instance.user_table[user_slot].is_active = 0;
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(fs_instance.header.user_table_offset);
    file.write(reinterpret_cast<const char*>(fs_instance.user_table.data()), fs_instance.header.max_users * sizeof(UserInfo));
    file.close();
}

std::vector<std::string> user_list(FileSystemInstance& fs_instance) {
    std::vector<std::string> active_users;
    for (const auto& user : fs_instance.user_table) {
        if (user.is_active == 1) { active_users.push_back(user.username); }
    }
    return active_users;
}

std::vector<DirEntryInfo> dir_list(FileSystemInstance& fs_instance, const std::string& path) {
    std::vector<DirEntryInfo> results;
    int parent_index = find_entry_by_path(fs_instance, path);
    if (parent_index == -1) return results;
    for (const auto& entry : fs_instance.metadata_entries) {
        if (entry.validity_flag == 0 && entry.parent_index == (uint32_t)parent_index) {
            results.push_back({entry.short_name, (entry.type_flag == 1)});
        }
    }
    return results;
}

void file_create(FileSystemInstance& fs_instance, const std::string& path, const std::string& content) {
    std::string parent_path = "/";
    std::string filename = path;
    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
        parent_path = path.substr(0, last_slash);
        if (parent_path.empty()) parent_path = "/";
        filename = path.substr(last_slash + 1);
    } else if (path.length() > 0 && path[0] == '/') {
        filename = path.substr(1);
    }

    int parent_index = find_entry_by_path(fs_instance, parent_path);
    if (parent_index == -1) return;
    int free_entry_index = find_free_metadata_entry(fs_instance);
    if (free_entry_index == -1) return;
    int free_block_index = find_free_block(fs_instance);
    if (free_block_index == -1) return;

    MetadataEntry& new_file = fs_instance.metadata_entries[free_entry_index];
    new_file = {};
    new_file.validity_flag = 0; new_file.type_flag = 0;
    new_file.parent_index = parent_index;
    strncpy(new_file.short_name, filename.c_str(), sizeof(new_file.short_name) - 1);
    new_file.total_size = content.length();
    new_file.start_index = free_block_index;
    new_file.created_time = time(nullptr);
    new_file.modified_time = time(nullptr);
    fs_instance.free_block_map[free_block_index] = false;

    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    long meta_position = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (free_entry_index * sizeof(MetadataEntry));
    file.seekp(meta_position);
    file.write(reinterpret_cast<const char*>(&new_file), sizeof(MetadataEntry));
    long data_area_start = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (1000 * sizeof(MetadataEntry));
    long data_position = data_area_start + ((free_block_index - 1) * fs_instance.header.block_size);
    file.seekp(data_position);
    file.write(content.c_str(), content.length());
    file.close();
}

std::string file_read(FileSystemInstance& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index == -1) return "";
    const auto& entry = fs_instance.metadata_entries[entry_index];
    if (entry.type_flag == 1) return "";
    std::ifstream ifs(fs_instance.omni_file_path, std::ios::binary);
    long data_area_start = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (1000 * sizeof(MetadataEntry));
    long data_position = data_area_start + ((entry.start_index - 1) * fs_instance.header.block_size);
    ifs.seekg(data_position);
    std::vector<char> buffer(entry.total_size);
    ifs.read(buffer.data(), entry.total_size);
    return std::string(buffer.begin(), buffer.end());
}

void dir_create(FileSystemInstance& fs_instance, const std::string& path) {
    std::string parent_path = "/";
    std::string dirname = path;
    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
        parent_path = path.substr(0, last_slash);
        if (parent_path.empty()) parent_path = "/";
        dirname = path.substr(last_slash + 1);
    } else if (path.length() > 0 && path[0] == '/') {
         dirname = path.substr(1);
    }
    int parent_index = find_entry_by_path(fs_instance, parent_path);
    if (parent_index == -1) return;
    int free_entry_index = find_free_metadata_entry(fs_instance);
    if (free_entry_index == -1) return;

    MetadataEntry& new_dir = fs_instance.metadata_entries[free_entry_index];
    new_dir = {};
    new_dir.validity_flag = 0; new_dir.type_flag = 1;
    new_dir.parent_index = parent_index;
    strncpy(new_dir.short_name, dirname.c_str(), sizeof(new_dir.short_name) - 1);
    new_dir.created_time = time(nullptr);
    new_dir.modified_time = time(nullptr);

    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    long meta_position = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (free_entry_index * sizeof(MetadataEntry));
    file.seekp(meta_position);
    file.write(reinterpret_cast<const char*>(&new_dir), sizeof(MetadataEntry));
    file.close();
}

void file_delete(FileSystemInstance& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index == -1) return;
    int block_to_free = fs_instance.metadata_entries[entry_index].start_index;
    fs_instance.free_block_map[block_to_free] = true;
    fs_instance.metadata_entries[entry_index].validity_flag = 1;
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    long position = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&fs_instance.metadata_entries[entry_index]), sizeof(MetadataEntry));
    file.close();
}

void dir_delete(FileSystemInstance& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index == -1 || entry_index == 0) return;
    for (const auto& entry : fs_instance.metadata_entries) {
        if (entry.validity_flag == 0 && entry.parent_index == (uint32_t)entry_index) return;
    }
    fs_instance.metadata_entries[entry_index].validity_flag = 1;
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    long position = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&fs_instance.metadata_entries[entry_index]), sizeof(MetadataEntry));
    file.close();
}

bool file_exists(FileSystemInstance& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    return (entry_index != -1 && fs_instance.metadata_entries[entry_index].type_flag == 0);
}

bool dir_exists(FileSystemInstance& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    return (entry_index != -1 && fs_instance.metadata_entries[entry_index].type_flag == 1);
}

void file_rename(FileSystemInstance& fs_instance, const std::string& old_path, const std::string& new_path) {
    int entry_index = find_entry_by_path(fs_instance, old_path);
    if (entry_index == -1 || entry_index == 0) return;
    std::string new_parent_path = "/";
    std::string new_name = new_path;
    size_t last_slash = new_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        new_parent_path = new_path.substr(0, last_slash);
        if (new_parent_path.empty()) new_parent_path = "/";
        new_name = new_path.substr(last_slash + 1);
    } else if (new_path[0] == '/') {
        new_name = new_path.substr(1);
    }
    int new_parent_index = find_entry_by_path(fs_instance, new_parent_path);
    if (new_parent_index == -1) return;
    MetadataEntry& entry_to_move = fs_instance.metadata_entries[entry_index];
    entry_to_move.parent_index = new_parent_index;
    strncpy(entry_to_move.short_name, new_name.c_str(), sizeof(entry_to_move.short_name) - 1);
    entry_to_move.modified_time = time(nullptr);
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    long position = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&entry_to_move), sizeof(MetadataEntry));
    file.close();
}

int find_free_metadata_entry(FileSystemInstance& fs_instance) {
    for (size_t i = 1; i < fs_instance.metadata_entries.size(); ++i) {
        if (fs_instance.metadata_entries[i].validity_flag == 1) return i;
    }
    return -1;
}

int find_free_block(FileSystemInstance& fs_instance) {
    for (size_t i = 1; i < fs_instance.free_block_map.size(); ++i) {
        if (fs_instance.free_block_map[i] == true) return i;
    }
    return -1;
}

int find_entry_by_path(FileSystemInstance& fs_instance, const std::string& path) {
    if (path == "/" || path.empty()) return 0;
    std::string temp_path = (path[0] == '/') ? path.substr(1) : path;
    std::stringstream ss(temp_path);
    std::string segment;
    std::vector<std::string> segments;
    while(std::getline(ss, segment, '/')) {
       if (!segment.empty()) segments.push_back(segment);
    }
    if (segments.empty()) return 0;
    
    int current_parent_index = 0;
    for (size_t i = 0; i < segments.size(); ++i) {
        const std::string& current_segment = segments[i];
        bool found_next = false;
        
        for (size_t j = 0; j < fs_instance.metadata_entries.size(); ++j) {
            const auto& entry = fs_instance.metadata_entries[j];
            if (entry.validity_flag == 0 && entry.parent_index == (uint32_t)current_parent_index && strcmp(entry.short_name, current_segment.c_str()) == 0) {
                if (i == segments.size() - 1) return j;
                if (entry.type_flag == 1) {
                    current_parent_index = j; found_next = true; break;
                }
            }
        }
        if (!found_next) return -1;
    }
    return -1;
}