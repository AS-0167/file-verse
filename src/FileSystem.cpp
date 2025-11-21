#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <ctime>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <random>
#include <algorithm>

#include "../include/FileSystem.h"
#include "../include/UserMap.h"

// --- Helper Function Prototypes ---
int find_entry_by_path(OFSystem& fs_instance, const std::string& path);
int find_free_metadata_entry(OFSystem& fs_instance);
int find_free_block(OFSystem& fs_instance);
std::string generate_session_id();

// ============================================================================
// CORE SYSTEM FUNCTIONS
// ============================================================================

void format_filesystem(const std::string& filepath) {
    std::cout << "Formatting new filesystem: " << filepath << std::endl;
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
    header.file_state_storage_offset = header.user_table_offset + (MAX_USERS * sizeof(UserInfo));

    std::vector<UserInfo> user_table(MAX_USERS, UserInfo{});
    UserInfo& admin_user = user_table[0];
    strncpy(admin_user.username, "admin", sizeof(admin_user.username) - 1);
    strncpy(admin_user.password_hash, "admin123", sizeof(admin_user.password_hash) - 1);
    admin_user.role = 1; admin_user.is_active = 1; admin_user.created_time = time(nullptr);

    std::vector<MetadataEntry> metadata_table(METADATA_COUNT, MetadataEntry{});
    for(size_t i = 0; i < METADATA_COUNT; ++i) { metadata_table[i].validity_flag = 1; }
    
    MetadataEntry& root_dir = metadata_table[0];
    root_dir.validity_flag = 0; root_dir.type_flag = 1; root_dir.parent_index = 0;
    strncpy(root_dir.short_name, "/", sizeof(root_dir.short_name) - 1);
    root_dir.owner_id = 0; root_dir.created_time = time(nullptr); root_dir.modified_time = time(nullptr);
    
    std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(OMNIHeader));
    ofs.write(reinterpret_cast<const char*>(user_table.data()), MAX_USERS * sizeof(UserInfo));
    ofs.write(reinterpret_cast<const char*>(metadata_table.data()), METADATA_COUNT * sizeof(MetadataEntry));
    
    uint64_t current_size = ofs.tellp();
    std::vector<char> empty_space(TOTAL_FS_SIZE - current_size, 0);
    ofs.write(empty_space.data(), empty_space.size());
    ofs.close();
    std::cout << "Format complete!" << std::endl;
}

void init_filesystem(OFSystem& fs_instance, const std::string& filepath) {
    std::cout << "\nInitializing file system from: " << filepath << std::endl;
    fs_instance.omni_filepath = filepath;
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) { std::cerr << "Error opening file: " << filepath << std::endl; exit(1); }
    
    ifs.read(reinterpret_cast<char*>(&fs_instance.header), sizeof(OMNIHeader));
    fs_instance.user_table.resize(fs_instance.header.max_users);
    ifs.read(reinterpret_cast<char*>(fs_instance.user_table.data()), fs_instance.header.max_users * sizeof(UserInfo));
    
    fs_instance.user_map = user_map_create(fs_instance.header.max_users);
    for (size_t i = 0; i < fs_instance.user_table.size(); ++i) {
        if (fs_instance.user_table[i].is_active == 1) {
            user_map_insert(fs_instance.user_map, fs_instance.user_table[i].username, &fs_instance.user_table[i]);
        }
    }
    
    const uint32_t METADATA_COUNT = 1000;
    fs_instance.metadata_entries.resize(METADATA_COUNT);
    ifs.seekg(fs_instance.header.file_state_storage_offset);
    ifs.read(reinterpret_cast<char*>(fs_instance.metadata_entries.data()), METADATA_COUNT * sizeof(MetadataEntry));
    
    uint64_t data_area_start = fs_instance.header.file_state_storage_offset + (METADATA_COUNT * sizeof(MetadataEntry));
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
    std::cout << "File system loaded into memory." << std::endl;
}

void shutdown_filesystem() {
    std::cout << "\n--- Shutting down server ---" << std::endl;
    exit(0);
}

// ============================================================================
// USER MANAGEMENT
// ============================================================================
std::string login_user(OFSystem& fs_instance, const std::string& username, const std::string& password) {
    std::cout << "\n--- Attempting Login for user: " << username << " ---" << std::endl;
    UserInfo* user = user_map_get(fs_instance.user_map, username);
    if (user != nullptr && strcmp(user->password_hash, password.c_str()) == 0) {
        std::cout << "Login successful! Generating session..." << std::endl;
        std::string session_id = generate_session_id();
        fs_instance.active_sessions[session_id] = user;
        user->last_login = time(nullptr);
        return session_id;
    }
    std::cout << "Login failed: Invalid username or password." << std::endl;
    return "";
}

void logout_user(OFSystem& fs_instance, const std::string& session_id) {
    std::cout << "\n--- Logging out session: " << session_id << " ---" << std::endl;
    if (fs_instance.active_sessions.erase(session_id) > 0) {
        std::cout << "Session successfully logged out." << std::endl;
    } else {
        std::cout << "Warning: Logout for a session that does not exist." << std::endl;
    }
}

void create_user(OFSystem& fs_instance, const std::string& username, const std::string& password, uint32_t role) {
    std::cout << "\n--- Creating new user: " << username << " ---" << std::endl;
    int free_slot = -1;
    for (size_t i = 0; i < fs_instance.user_table.size(); ++i) {
        if (fs_instance.user_table[i].is_active == 0) {
            free_slot = i;
            break;
        }
    }
    if (free_slot == -1) { std::cout << "Error: No free user slots available." << std::endl; return; }
    
    UserInfo& new_user = fs_instance.user_table[free_slot];
    new_user.is_active = 1;
    new_user.role = role;
    strncpy(new_user.username, username.c_str(), sizeof(new_user.username) - 1);
    strncpy(new_user.password_hash, password.c_str(), sizeof(new_user.password_hash) - 1);
    new_user.created_time = time(nullptr);
    
    user_map_insert(fs_instance.user_map, new_user.username, &new_user);
    
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(fs_instance.header.user_table_offset);
    file.write(reinterpret_cast<const char*>(fs_instance.user_table.data()), fs_instance.header.max_users * sizeof(UserInfo));
    file.close();
    std::cout << "Successfully created user '" << username << "'." << std::endl;
}

void delete_user(OFSystem& fs_instance, const std::string& username) {
    std::cout << "\n--- Deleting user: " << username << " ---" << std::endl;
    if (username == "admin") { std::cout << "Error: Cannot delete the admin user." << std::endl; return; }
    
    int user_slot = -1;
    for (size_t i = 0; i < fs_instance.user_table.size(); ++i) {
        if (fs_instance.user_table[i].is_active == 1 && strcmp(fs_instance.user_table[i].username, username.c_str()) == 0) {
            user_slot = i;
            break;
        }
    }
    if (user_slot == -1) { std::cout << "Error: User '" << username << "' not found." << std::endl; return; }
    
    fs_instance.user_table[user_slot].is_active = 0;
    
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(fs_instance.header.user_table_offset + (user_slot * sizeof(UserInfo)));
    file.write(reinterpret_cast<const char*>(&fs_instance.user_table[user_slot]), sizeof(UserInfo));
    file.close();
    
    std::cout << "Successfully deleted user '" << username << "'." << std::endl;
}

std::vector<std::string> list_all_users(OFSystem& fs_instance) {
    std::vector<std::string> users;
    for (const auto& user : fs_instance.user_table) {
        if (user.is_active == 1) {
            users.push_back(user.username);
        }
    }
    return users;
}

SessionInfo get_session_details(OFSystem& fs_instance, const std::string& session_id) {
    auto it = fs_instance.active_sessions.find(session_id);
    if (it != fs_instance.active_sessions.end()) {
        return {it->second->username, it->second->role};
    }
    return {};
}

// ============================================================================
// DIRECTORY AND FILE OPERATIONS
// ============================================================================
void create_directory(OFSystem& fs_instance, const std::string& path) {
    std::cout << "\n--- Creating Directory: " << path << " ---" << std::endl;
    std::string parent_path = "/"; std::string dirname = path;
    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
        parent_path = path.substr(0, last_slash);
        if (parent_path.empty()) parent_path = "/";
        dirname = path.substr(last_slash + 1);
    } else if (path.length() > 1 && path[0] == '/') { dirname = path.substr(1); }
    int parent_index = find_entry_by_path(fs_instance, parent_path);
    if (parent_index == -1) { std::cout << "Error: Parent directory '" << parent_path << "' not found." << std::endl; return; }
    int free_entry_index = find_free_metadata_entry(fs_instance);
    if (free_entry_index == -1) return;
    MetadataEntry& new_dir = fs_instance.metadata_entries[free_entry_index];
    new_dir.validity_flag = 0; new_dir.type_flag = 1; new_dir.parent_index = parent_index;
    strncpy(new_dir.short_name, dirname.c_str(), sizeof(new_dir.short_name) - 1);
    new_dir.total_size = 0; new_dir.start_index = 0;
    new_dir.created_time = time(nullptr); new_dir.modified_time = time(nullptr);
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    long meta_position = fs_instance.header.file_state_storage_offset + (free_entry_index * sizeof(MetadataEntry));
    file.seekp(meta_position);
    file.write(reinterpret_cast<const char*>(&new_dir), sizeof(MetadataEntry));
    file.close();
}

std::vector<DirEntryInfo> list_directory_contents(OFSystem& fs_instance, const std::string& path) {
    std::vector<DirEntryInfo> results;
    int parent_index = find_entry_by_path(fs_instance, path);
    if (parent_index == -1) { std::cout << "Error: Directory '" << path << "' not found." << std::endl; return results; }
    for (const auto& entry : fs_instance.metadata_entries) {
        if (entry.validity_flag == 0 && entry.parent_index == (uint32_t)parent_index) {
            results.push_back({entry.short_name, (entry.type_flag == 1)});
        }
    }
    return results;
}

void remove_directory(OFSystem& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index == -1 || entry_index == 0) { std::cout << "Error: Directory not found or cannot delete root." << std::endl; return; }
    for (const auto& entry : fs_instance.metadata_entries) {
        if (entry.validity_flag == 0 && entry.parent_index == (uint32_t)entry_index) {
            std::cout << "Error: Directory is not empty." << std::endl; return;
        }
    }
    fs_instance.metadata_entries[entry_index].validity_flag = 1;
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    long position = fs_instance.header.file_state_storage_offset + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&fs_instance.metadata_entries[entry_index]), sizeof(MetadataEntry));
    file.close();
}

bool path_is_directory(OFSystem& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    return (entry_index != -1 && fs_instance.metadata_entries[entry_index].type_flag == 1);
}

void create_file_with_content(OFSystem& fs_instance, const std::string& path, const std::string& content) {
    std::string parent_path = "/"; std::string filename = path;
    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
        parent_path = path.substr(0, last_slash);
        if (parent_path.empty()) parent_path = "/";
        filename = path.substr(last_slash + 1);
    } else if (path.length() > 0 && path[0] == '/') { filename = path.substr(1); }
    int parent_index = find_entry_by_path(fs_instance, parent_path);
    if (parent_index == -1) { std::cout << "Error: Parent directory '" << parent_path << "' not found." << std::endl; return; }
    int free_entry_index = find_free_metadata_entry(fs_instance);
    if (free_entry_index == -1) return;
    int free_block_index = find_free_block(fs_instance);
    if (free_block_index == -1) return;
    MetadataEntry& new_file = fs_instance.metadata_entries[free_entry_index];
    new_file.validity_flag = 0; new_file.type_flag = 0; new_file.parent_index = parent_index;
    strncpy(new_file.short_name, filename.c_str(), sizeof(new_file.short_name) - 1);
    new_file.total_size = content.length(); new_file.start_index = free_block_index;
    new_file.created_time = time(nullptr); new_file.modified_time = time(nullptr);
    fs_instance.free_block_map[free_block_index] = false;
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    long meta_position = fs_instance.header.file_state_storage_offset + (free_entry_index * sizeof(MetadataEntry));
    file.seekp(meta_position);
    file.write(reinterpret_cast<const char*>(&new_file), sizeof(MetadataEntry));
    long data_area_start = fs_instance.header.file_state_storage_offset + (1000 * sizeof(MetadataEntry));
    long data_position = data_area_start + (free_block_index * fs_instance.header.block_size);
    file.seekp(data_position);
    file.write(content.c_str(), content.length());
    file.close();
}

std::string read_file_content(OFSystem& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index != -1) {
        const auto& entry = fs_instance.metadata_entries[entry_index];
        if (entry.type_flag == 1) { std::cout << "Error: Cannot read a directory." << std::endl; return ""; }
        std::ifstream ifs(fs_instance.omni_filepath, std::ios::binary);
        long data_area_start = fs_instance.header.file_state_storage_offset + (1000 * sizeof(MetadataEntry));
        long data_position = data_area_start + (entry.start_index * fs_instance.header.block_size);
        ifs.seekg(data_position);
        std::vector<char> buffer(entry.total_size);
        ifs.read(buffer.data(), entry.total_size);
        return std::string(buffer.data(), entry.total_size);
    }
    std::cout << "File not found at path: " << path << std::endl;
    return "";
}

void remove_file(OFSystem& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index == -1) { std::cout << "Error: File '" << path << "' not found." << std::endl; return; }
    int block_to_free = fs_instance.metadata_entries[entry_index].start_index;
    fs_instance.free_block_map[block_to_free] = true;
    fs_instance.metadata_entries[entry_index].validity_flag = 1;
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    long position = fs_instance.header.file_state_storage_offset + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&fs_instance.metadata_entries[entry_index]), sizeof(MetadataEntry));
    file.close();
}

void edit_file(OFSystem& fs_instance, const std::string& path, const std::string& new_content, uint32_t index) {
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index == -1) { std::cout << "Error: File '" << path << "' not found." << std::endl; return; }
    MetadataEntry& entry = fs_instance.metadata_entries[entry_index];
    if (entry.type_flag == 1) { std::cout << "Error: Cannot edit a directory." << std::endl; return; }
    if (index + new_content.length() > entry.total_size) { std::cout << "Error: Edit exceeds the original file size." << std::endl; return; }
    long data_area_start = fs_instance.header.file_state_storage_offset + (1000 * sizeof(MetadataEntry));
    long block_start_pos = data_area_start + (entry.start_index * fs_instance.header.block_size);
    long final_write_pos = block_start_pos + index;
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(final_write_pos);
    file.write(new_content.c_str(), new_content.length());
    entry.modified_time = time(nullptr);
    file.seekp(fs_instance.header.file_state_storage_offset + (entry_index * sizeof(MetadataEntry)));
    file.write(reinterpret_cast<const char*>(&entry), sizeof(MetadataEntry));
    file.close();
}

void truncate_file_content(OFSystem& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index == -1) { std::cout << "Error: File '" << path << "' not found." << std::endl; return; }
    MetadataEntry& entry = fs_instance.metadata_entries[entry_index];
    if (entry.type_flag == 1) { std::cout << "Error: Cannot truncate a directory." << std::endl; return; }
    entry.total_size = 0;
    entry.modified_time = time(nullptr);
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    long position = fs_instance.header.file_state_storage_offset + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&entry), sizeof(MetadataEntry));
    file.close();
}

bool path_is_file(OFSystem& fs_instance, const std::string& path) {
    int entry_index = find_entry_by_path(fs_instance, path);
    return (entry_index != -1 && fs_instance.metadata_entries[entry_index].type_flag == 0);
}

void rename_path(OFSystem& fs_instance, const std::string& old_path, const std::string& new_path) {
    int entry_index = find_entry_by_path(fs_instance, old_path);
    if (entry_index == -1 || entry_index == 0) { std::cout << "Error: Source file/directory not found or is root." << std::endl; return; }
    std::string new_parent_path = "/"; std::string new_name = new_path;
    size_t last_slash = new_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        new_parent_path = new_path.substr(0, last_slash);
        if (new_parent_path.empty()) new_parent_path = "/";
        new_name = new_path.substr(last_slash + 1);
    } else if (new_path[0] == '/') { new_name = new_path.substr(1); }
    int new_parent_index = find_entry_by_path(fs_instance, new_parent_path);
    if (new_parent_index == -1) { std::cout << "Error: Destination directory '" << new_parent_path << "' not found." << std::endl; return; }
    MetadataEntry& entry_to_move = fs_instance.metadata_entries[entry_index];
    entry_to_move.parent_index = new_parent_index;
    strncpy(entry_to_move.short_name, new_name.c_str(), sizeof(entry_to_move.short_name) - 1);
    entry_to_move.modified_time = time(nullptr);
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    long position = fs_instance.header.file_state_storage_offset + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&entry_to_move), sizeof(MetadataEntry));
    file.close();
}

FSStats get_fs_stats(OFSystem& fs_instance) {
    FSStats stats = {};
    stats.total_size = fs_instance.header.total_size;
    uint64_t occupied_blocks = 0;
    for (const auto& entry : fs_instance.metadata_entries) {
        if (entry.validity_flag == 0) {
            if (entry.type_flag == 0) {
                stats.file_count++;
                if (entry.total_size > 0) {
                    occupied_blocks += (entry.total_size - 1) / fs_instance.header.block_size + 1;
                }
            } else {
                stats.directory_count++;
            }
        }
    }
    stats.used_space = occupied_blocks * fs_instance.header.block_size;
    stats.free_space = stats.total_size - (fs_instance.header.file_state_storage_offset + (1000 * sizeof(MetadataEntry))) - stats.used_space;
    return stats;
}

FileMetadata get_path_metadata(OFSystem& fs_instance, const std::string& path) {
    FileMetadata meta = {};
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index != -1) {
        const MetadataEntry& entry = fs_instance.metadata_entries[entry_index];
        meta.name = entry.short_name;
        meta.is_directory = (entry.type_flag == 1);
        meta.size = entry.total_size;
        meta.owner_id = entry.owner_id;
        meta.permissions = entry.permissions;
        meta.created_time = entry.created_time;
        meta.modified_time = entry.modified_time;
    }
    return meta;
}

void set_path_permissions(OFSystem& fs_instance, const std::string& path, uint32_t permissions) {
    int entry_index = find_entry_by_path(fs_instance, path);
    if (entry_index == -1) { return; }
    MetadataEntry& entry = fs_instance.metadata_entries[entry_index];
    entry.permissions = permissions;
    entry.modified_time = time(nullptr);
    std::fstream file(fs_instance.omni_filepath, std::ios::in | std::ios::out | std::ios::binary);
    long position = fs_instance.header.file_state_storage_offset + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&entry), sizeof(MetadataEntry));
    file.close();
}

std::string get_error_string(int error_code) {
    switch (error_code) {
        case 401: return "Out of Range: Array index is out of range.";
        case 403: return "Out of Range: Key not found in object.";
        case 302: return "Type Error: Incompatible type.";
        case 304: return "Type Error: Cannot use 'at' with this type.";
        case 305: return "Type Error: Cannot use 'operator[]' with this type.";
        case 307: return "Type Error: Cannot use 'erase' with this type.";
        case 101: return "Parse Error: An unexpected token was found.";
        case 104: return "Parse Error: JSON patch must be an array of objects.";
        case 105: return "Parse Error: Patch operation is missing a required member.";
        default: return "An unknown error occurred.";
    }
}

// ============================================================================
// HELPER FUNCTION IMPLEMENTATIONS
// ============================================================================
int find_free_metadata_entry(OFSystem& fs_instance) {
    for (size_t i = 1; i < fs_instance.metadata_entries.size(); ++i) {
        if (fs_instance.metadata_entries[i].validity_flag == 1) { return i; }
    }
    std::cerr << "Error: No free metadata entries available!" << std::endl;
    return -1;
}

int find_free_block(OFSystem& fs_instance) {
    for (size_t i = 1; i < fs_instance.free_block_map.size(); ++i) {
        if (fs_instance.free_block_map[i] == true) { return i; }
    }
    std::cerr << "Error: No free data blocks available!" << std::endl;
    return -1;
}

int find_entry_by_path(OFSystem& fs_instance, const std::string& path) {
    if (path == "/" || path.empty()) { return 0; }
    std::vector<std::string> segments;
    std::string temp_path = path;
    if (temp_path[0] == '/') { temp_path = temp_path.substr(1); }
    if (!temp_path.empty() && temp_path.back() == '/') { temp_path.pop_back(); }
    
    std::stringstream ss(temp_path);
    std::string segment;
    while(std::getline(ss, segment, '/')) {
       if (!segment.empty()) { segments.push_back(segment); }
    }
    
    if (segments.empty()) { return 0; }
    
    int current_parent_index = 0;
    for (size_t i = 0; i < segments.size(); ++i) {
        const std::string& current_segment = segments[i];
        bool found_next = false;
        for (size_t j = 1; j < fs_instance.metadata_entries.size(); ++j) {
            const auto& entry = fs_instance.metadata_entries[j];
            if (entry.validity_flag == 0 && entry.parent_index == (uint32_t)current_parent_index && strcmp(entry.short_name, current_segment.c_str()) == 0) {
                if (i == segments.size() - 1) { return j; }
                if (entry.type_flag == 1) {
                    current_parent_index = j;
                    found_next = true;
                    break;
                }
            }
        }
        if (!found_next) { return -1; }
    }
    return -1;
}

std::string generate_session_id() {
    std::string id;
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, sizeof(alphanum) - 2);
    for (int i = 0; i < 32; ++i) {
        id += alphanum[distrib(gen)];
    }
    return id;
}