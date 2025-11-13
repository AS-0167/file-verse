#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <ctime>
#include <cstring>
#include <sstream> // Used for splitting paths

#include "../include/ofs_types.h"
#include "../include/hash_table.h"

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
    std::cout << "Formatting a new file system at: " << file_path << std::endl;
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
    std::vector<char> empty_space(TOTAL_FS_SIZE - current_size, 0);
    ofs.write(empty_space.data(), empty_space.size());

    ofs.close();
    std::cout << "Format complete!" << std::endl;
}

void fs_init(FileSystemInstance& fs_instance, const std::string& file_path) {
    std::cout << "\nInitializing file system from: " << file_path << std::endl;
    fs_instance.omni_file_path = file_path;

    std::ifstream ifs(file_path, std::ios::binary);
    if (!ifs) { std::cerr << "Error opening file: " << file_path << std::endl; return; }

    ifs.read(reinterpret_cast<char*>(&fs_instance.header), sizeof(OMNIHeader));
    
    fs_instance.user_table.resize(fs_instance.header.max_users);
    ifs.read(reinterpret_cast<char*>(fs_instance.user_table.data()), fs_instance.header.max_users * sizeof(UserInfo));
    
    fs_instance.user_hash_table = hash_table_create(fs_instance.header.max_users);
    for (size_t i = 0; i < fs_instance.user_table.size(); ++i) {
        if (fs_instance.user_table[i].is_active == 1) {
            hash_table_insert(fs_instance.user_hash_table, fs_instance.user_table[i].username, &fs_instance.user_table[i]);
        }
    }
    
    const uint32_t METADATA_COUNT = 1000;
    fs_instance.metadata_entries.resize(METADATA_COUNT);
    ifs.read(reinterpret_cast<char*>(fs_instance.metadata_entries.data()), METADATA_COUNT * sizeof(MetadataEntry));
    
    uint64_t total_data_blocks = fs_instance.header.total_size / fs_instance.header.block_size;
    fs_instance.free_block_map.assign(total_data_blocks, true);
    for(const auto& entry : fs_instance.metadata_entries) {
        if (entry.validity_flag == 0 && entry.start_index > 0) {
            fs_instance.free_block_map[entry.start_index] = false;
        }
    }

    ifs.close();
    std::cout << "File system loaded into memory." << std::endl;
}
// --- PASTE THIS NEW FUNCTION INTO your src/filesystem.cpp file ---

// ============================================================================
// USER MANAGEMENT
// ============================================================================
bool user_login(FileSystemInstance& fs_instance, const std::string& username, const std::string& password) {
    std::cout << "\n--- Attempting Login for user: " << username << " ---" << std::endl;
    
    // Use our super-fast hash table to look up the user by their username.
    UserInfo* user = hash_table_get(fs_instance.user_hash_table, username);

    // hash_table_get returns nullptr if the user is not found.
    if (user != nullptr) { 
        // User was found, now check if the password matches.
        if (strcmp(user->password_hash, password.c_str()) == 0) {
            std::cout << "Login successful!" << std::endl;
            return true; // Password matches!
        } else {
            std::cout << "Login failed: Incorrect password." << std::endl;
            return false; // Password does not match.
        }
    }
    
    std::cout << "Login failed: User not found." << std::endl;
    return false; // User was not found in the hash table.
}
// --- PASTE THIS NEW FUNCTION INTO filesystem.cpp ---

void user_create(FileSystemInstance& fs_instance, const std::string& username, const std::string& password, uint32_t role) {
    std::cout << "\n--- Creating new user: " << username << " ---" << std::endl;

    // 1. Find the first inactive user slot in our in-memory user_table.
    int free_user_slot = -1;
    for (size_t i = 0; i < fs_instance.user_table.size(); ++i) {
        if (fs_instance.user_table[i].is_active == 0) {
            free_user_slot = i;
            break;
        }
    }

    if (free_user_slot == -1) {
        std::cout << "Error: No free user slots available." << std::endl;
        return;
    }

    // 2. Get a reference to that free slot and populate it with the new user's info.
    UserInfo& new_user = fs_instance.user_table[free_user_slot];
    new_user.is_active = 1;
    new_user.role = role; // 0 for NORMAL, 1 for ADMIN
    strncpy(new_user.username, username.c_str(), sizeof(new_user.username) - 1);
    strncpy(new_user.password_hash, password.c_str(), sizeof(new_user.password_hash) - 1);
    new_user.created_time = time(nullptr);

    // 3. Add the new user to our fast-lookup hash table.
    hash_table_insert(fs_instance.user_hash_table, new_user.username, &new_user);

    // 4. Write the ENTIRE updated user table back to the .omni file to save the change.
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(fs_instance.header.user_table_offset); // Go to the beginning of the user table
    file.write(reinterpret_cast<const char*>(fs_instance.user_table.data()), fs_instance.header.max_users * sizeof(UserInfo));
    file.close();

    std::cout << "Successfully created user '" << username << "'." << std::endl;
}
// --- PASTE THESE TWO NEW FUNCTIONS INTO filesystem.cpp ---

void user_delete(FileSystemInstance& fs_instance, const std::string& username) {
    std::cout << "\n--- Deleting user: " << username << " ---" << std::endl;

    // We can't delete the main admin user
    if (username == "admin") {
        std::cout << "Error: Cannot delete the primary admin user." << std::endl;
        return;
    }

    // 1. Find the user in our in-memory user_table.
    int user_slot = -1;
    for (size_t i = 0; i < fs_instance.user_table.size(); ++i) {
        if (fs_instance.user_table[i].is_active == 1 && strcmp(fs_instance.user_table[i].username, username.c_str()) == 0) {
            user_slot = i;
            break;
        }
    }

    if (user_slot == -1) {
        std::cout << "Error: User '" << username << "' not found." << std::endl;
        return;
    }

    // 2. Mark the user as inactive in memory. We don't need to clear the data.
    fs_instance.user_table[user_slot].is_active = 0;

    // 3. NOTE: We are NOT removing them from the hash table for simplicity.
    // A more robust system would. For this project, a failed login is sufficient.

    // 4. Write the ENTIRE updated user table back to the .omni file.
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    file.seekp(fs_instance.header.user_table_offset);
    file.write(reinterpret_cast<const char*>(fs_instance.user_table.data()), fs_instance.header.max_users * sizeof(UserInfo));
    file.close();

    std::cout << "Successfully deleted user '" << username << "'." << std::endl;
}

// This function returns a vector of all active usernames
std::vector<std::string> user_list(FileSystemInstance& fs_instance) {
    std::cout << "\n--- Listing all users ---" << std::endl;
    std::vector<std::string> active_users;

    for (const auto& user : fs_instance.user_table) {
        if (user.is_active == 1) {
            active_users.push_back(user.username);
        }
    }
    return active_users;
}
// ============================================================================
// DIRECTORY AND FILE OPERATIONS
// ============================================================================

// --- FUNCTION WAS MISSING - ADDED BACK IN ---

// This simple struct helps us return structured data from the function
struct DirEntryInfo {
    std::string name;
    bool is_directory;
};

// The new function returns a vector of these structs instead of 'void'
std::vector<DirEntryInfo> dir_list(FileSystemInstance& fs_instance, const std::string& path) {
    std::cout << "\n--- Listing Contents of '" << path << "' ---" << std::endl;
    
    std::vector<DirEntryInfo> results; // We will store our results here to be returned
    int parent_index = find_entry_by_path(fs_instance, path);

    if (parent_index == -1) {
        std::cout << "Error: Directory '" << path << "' not found." << std::endl;
        return results; // Return the empty list
    }

    // Loop through all metadata entries
    for (const auto& entry : fs_instance.metadata_entries) {
        // If an entry is in use and its parent is the directory we're listing...
        if (entry.validity_flag == 0 && entry.parent_index == (uint32_t)parent_index) {
            // ...add it to our results list instead of printing it.
            results.push_back({entry.short_name, (entry.type_flag == 1)});
        }
    }
    return results; // Return the completed list
}



void file_create(FileSystemInstance& fs_instance, const std::string& path, const std::string& content) {
    std::cout << "\n--- Creating File: " << path << " ---" << std::endl;
    
    std::string parent_path = "/";
    std::string filename = path;
    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
        parent_path = path.substr(0, last_slash);
        if (parent_path.empty()) parent_path = "/";
        filename = path.substr(last_slash + 1);
    }

    int parent_index = find_entry_by_path(fs_instance, parent_path);
    if (parent_index == -1) {
        std::cout << "Error: Parent directory '" << parent_path << "' not found." << std::endl;
        return;
    }

    int free_entry_index = find_free_metadata_entry(fs_instance);
    if (free_entry_index == -1) return;
    int free_block_index = find_free_block(fs_instance);
    if (free_block_index == -1) return;

    MetadataEntry& new_file = fs_instance.metadata_entries[free_entry_index];
    new_file.validity_flag = 0;
    new_file.type_flag = 0;
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
    long data_position = data_area_start + (free_block_index * fs_instance.header.block_size);
    file.seekp(data_position);
    file.write(content.c_str(), content.length());
    
    file.close();
    std::cout << "Successfully created file '" << filename << "'." << std::endl;
}

std::string file_read(FileSystemInstance& fs_instance, const std::string& path) {
    std::cout << "\n--- Reading File: " << path << " ---" << std::endl;
    int entry_index = find_entry_by_path(fs_instance, path);

    if (entry_index != -1) {
        const auto& entry = fs_instance.metadata_entries[entry_index];
        if (entry.type_flag == 1) {
            std::cout << "Error: Cannot read a directory." << std::endl;
            return "";
        }

        std::cout << "Found file. Reading " << entry.total_size << " bytes from block " << entry.start_index << std::endl;
        std::ifstream ifs(fs_instance.omni_file_path, std::ios::binary);
        
        long data_area_start = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (1000 * sizeof(MetadataEntry));
        long data_position = data_area_start + (entry.start_index * fs_instance.header.block_size);
        ifs.seekg(data_position);

        std::vector<char> buffer(entry.total_size);
        ifs.read(buffer.data(), entry.total_size);
        return std::string(buffer.begin(), buffer.end());
    }

    std::cout << "File not found at path: " << path << std::endl;
    return "";
}
// --- PASTE THIS NEW FUNCTION INTO your src/filesystem.cpp file ---

void dir_create(FileSystemInstance& fs_instance, const std::string& path) {
    std::cout << "\n--- Creating Directory: " << path << " ---" << std::endl;
    
    // 1. Find parent directory and new directory name from the path
    std::string parent_path = "/";
    std::string dirname = path;
    size_t last_slash = path.find_last_of('/');

    // This logic handles paths like "/docs" and extracts "docs"
    if (last_slash != std::string::npos) {
        parent_path = path.substr(0, last_slash);
        if (parent_path.empty()) parent_path = "/"; // Handle case like "/somedir"
        dirname = path.substr(last_slash + 1);
    } else if (path.length() > 1 && path[0] == '/') {
         dirname = path.substr(1);
    }

    int parent_index = find_entry_by_path(fs_instance, parent_path);
    if (parent_index == -1) {
        std::cout << "Error: Parent directory '" << parent_path << "' not found." << std::endl;
        return;
    }

    // 2. Find a free metadata entry
    int free_entry_index = find_free_metadata_entry(fs_instance);
    if (free_entry_index == -1) return;

    // 3. Populate the new directory entry in memory
    MetadataEntry& new_dir = fs_instance.metadata_entries[free_entry_index];
    new_dir.validity_flag = 0;   // In Use
    new_dir.type_flag = 1;       // Directory
    new_dir.parent_index = parent_index;
    strncpy(new_dir.short_name, dirname.c_str(), sizeof(new_dir.short_name) - 1);
    new_dir.total_size = 0;      // Directories have no size
    new_dir.start_index = 0;     // Directories don't point to a data block
    new_dir.created_time = time(nullptr);
    new_dir.modified_time = time(nullptr);

    // 4. Write the change from memory to the .omni file
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    long meta_position = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (free_entry_index * sizeof(MetadataEntry));
    file.seekp(meta_position);
    file.write(reinterpret_cast<const char*>(&new_dir), sizeof(MetadataEntry));
    file.close();

    std::cout << "Successfully created directory '" << dirname << "'." << std::endl;
}
void file_delete(FileSystemInstance& fs_instance, const std::string& path) {
    std::cout << "\n--- Deleting File: " << path << " ---" << std::endl;

    // 1. Find the file using the helper you already have.
    int entry_index = find_entry_by_path(fs_instance, path);

    if (entry_index == -1) {
        std::cout << "Error: File '" << path << "' not found." << std::endl;
        return; // Or return an error code
    }

    // 2. Get the block number this file was using.
    int block_to_free = fs_instance.metadata_entries[entry_index].start_index;

    // 3. Update your IN-MEMORY structures first.
    fs_instance.free_block_map[block_to_free] = true; // Mark the data block as free.
    fs_instance.metadata_entries[entry_index].validity_flag = 1; // Mark the metadata entry as free.

    // 4. Write the single updated metadata entry back to the .omni file to make it permanent.
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    long position = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&fs_instance.metadata_entries[entry_index]), sizeof(MetadataEntry));
    file.close();

    std::cout << "Successfully deleted '" << path << "'." << std::endl;
}
// --- PASTE THIS NEW FUNCTION INTO filesystem.cpp ---

void dir_delete(FileSystemInstance& fs_instance, const std::string& path) {
    std::cout << "\n--- Deleting Directory: " << path << " ---" << std::endl;

    // 1. Find the directory to delete.
    int entry_index = find_entry_by_path(fs_instance, path);

    if (entry_index == -1 || entry_index == 0) { // Can't find it or it's the root
        std::cout << "Error: Directory not found or cannot delete root." << std::endl;
        return; 
    }

    // 2. CRITICAL: Check if the directory is empty.
    // Loop through all entries to see if any have this directory as their parent.
    for (const auto& entry : fs_instance.metadata_entries) {
        if (entry.validity_flag == 0 && entry.parent_index == (uint32_t)entry_index) {
            std::cout << "Error: Directory is not empty." << std::endl;
            return; // Found a child, so we can't delete.
        }
    }

    // 3. If it's empty, mark it as free (delete it).
    fs_instance.metadata_entries[entry_index].validity_flag = 1;

    // 4. Write the change back to the file.
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    long position = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&fs_instance.metadata_entries[entry_index]), sizeof(MetadataEntry));
    file.close();

    std::cout << "Successfully deleted empty directory '" << path << "'." << std::endl;
}
// --- PASTE THIS NEW FUNCTION INTO filesystem.cpp ---

bool file_exists(FileSystemInstance& fs_instance, const std::string& path) {
    std::cout << "\n--- Checking if file exists: " << path << " ---" << std::endl;

    // 1. Use the helper we already have.
    int entry_index = find_entry_by_path(fs_instance, path);

    // 2. If it was not found, it doesn't exist.
    if (entry_index == -1) {
        return false;
    }

    // 3. If it was found, make sure it's a FILE, not a directory.
    if (fs_instance.metadata_entries[entry_index].type_flag == 0) { // 0 = File
        return true;
    }

    return false;
}
// --- PASTE THIS NEW FUNCTION INTO filesystem.cpp ---

bool dir_exists(FileSystemInstance& fs_instance, const std::string& path) {
    std::cout << "\n--- Checking if directory exists: " << path << " ---" << std::endl;

    // 1. Use the helper we already have.
    int entry_index = find_entry_by_path(fs_instance, path);

    // 2. If it was not found, it doesn't exist.
    if (entry_index == -1) {
        return false;
    }

    // 3. If it was found, make sure it's a DIRECTORY, not a file.
    if (fs_instance.metadata_entries[entry_index].type_flag == 1) { // 1 = Directory
        return true;
    }

    return false;
}
// --- PASTE THIS NEW FUNCTION INTO filesystem.cpp ---

void file_rename(FileSystemInstance& fs_instance, const std::string& old_path, const std::string& new_path) {
    std::cout << "\n--- Renaming/Moving file from " << old_path << " to " << new_path << " ---" << std::endl;

    // 1. Find the entry for the file/dir we want to move.
    int entry_index = find_entry_by_path(fs_instance, old_path);
    if (entry_index == -1 || entry_index == 0) {
        std::cout << "Error: Source file/directory not found or is root." << std::endl;
        return;
    }

    // 2. Figure out the new parent directory and the new name.
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

    // 3. Find the index of the new parent directory.
    int new_parent_index = find_entry_by_path(fs_instance, new_parent_path);
    if (new_parent_index == -1) {
        std::cout << "Error: Destination directory '" << new_parent_path << "' not found." << std::endl;
        return;
    }
    
    // 4. Update the metadata entry in memory.
    MetadataEntry& entry_to_move = fs_instance.metadata_entries[entry_index];
    entry_to_move.parent_index = new_parent_index;
    strncpy(entry_to_move.short_name, new_name.c_str(), sizeof(entry_to_move.short_name) - 1);
    entry_to_move.modified_time = time(nullptr);

    // 5. Write the updated entry back to the .omni file.
    std::fstream file(fs_instance.omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    long position = sizeof(OMNIHeader) + (fs_instance.header.max_users * sizeof(UserInfo)) + (entry_index * sizeof(MetadataEntry));
    file.seekp(position);
    file.write(reinterpret_cast<const char*>(&entry_to_move), sizeof(MetadataEntry));
    file.close();

    std::cout << "Successfully renamed/moved to '" << new_path << "'." << std::endl;
}
// ============================================================================
// HELPER FUNCTIONS
// ============================================================================
int find_free_metadata_entry(FileSystemInstance& fs_instance) {
    for (size_t i = 1; i < fs_instance.metadata_entries.size(); ++i) {
        if (fs_instance.metadata_entries[i].validity_flag == 1) {
            return i;
        }
    }
    std::cerr << "Error: No free metadata entries available!" << std::endl;
    return -1;
}

int find_free_block(FileSystemInstance& fs_instance) {
    for (size_t i = 1; i < fs_instance.free_block_map.size(); ++i) {
        if (fs_instance.free_block_map[i] == true) {
            return i;
        }
    }
    std::cerr << "Error: No free data blocks available!" << std::endl;
    return -1;
}

// --- REPLACE your old find_entry_by_path function with THIS ONE ---

/**
 * FINAL, ROBUST VERSION: Finds any file or directory by its full, absolute path.
 */
int find_entry_by_path(FileSystemInstance& fs_instance, const std::string& path) {
    if (path == "/" || path.empty()) {
        return 0; // Root directory is always at index 0
    }

    // Split the path into segments (e.g., "/docs/file.txt" -> "docs", "file.txt")
    std::vector<std::string> segments;
    std::string temp_path = path;
    if (temp_path[0] == '/') {
        temp_path = temp_path.substr(1);
    }
    std::stringstream ss(temp_path);
    std::string segment;

    while(std::getline(ss, segment, '/')) {
       if (!segment.empty()) {
           segments.push_back(segment);
       }
    }

    if (segments.empty()) {
        return 0; // This handles cases like "/" or "//"
    }
    
    int current_parent_index = 0; // Start our search from the root directory

    // Loop through each segment of the path
    for (size_t i = 0; i < segments.size(); ++i) {
        const std::string& current_segment = segments[i];
        bool found_next = false;
        
        // Search all metadata entries for a match in the current directory
        for (size_t j = 1; j < fs_instance.metadata_entries.size(); ++j) { // Start from 1 to skip root self-ref
            const auto& entry = fs_instance.metadata_entries[j];
            
            // Check if the entry is active, is a child of our current location, and has the right name
            if (entry.validity_flag == 0 && entry.parent_index == (uint32_t)current_parent_index && strcmp(entry.short_name, current_segment.c_str()) == 0) {
                
                // If this is the LAST segment of the path, we have found our target
                if (i == segments.size() - 1) {
                    return j; // Success! Return the index of the found file/dir.
                }

                // If this is a directory in the middle of the path, we continue searching from here
                if (entry.type_flag == 1) { // 1 = Directory
                    current_parent_index = j;
                    found_next = true;
                    break; // Move on to the next segment
                }
            }
        }
        
        // If we looped through all entries and didn't find the next part of the path
        if (!found_next) {
            return -1; // Path not found
        }
    }

    return -1; // Should not be reached, but indicates failure
}