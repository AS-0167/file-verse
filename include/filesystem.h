#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <vector> // <-- ADD THIS INCLUDE

#include <string>
#include "ofs_types.h" // We need this for the "FileSystemInstance" struct definition

// ============================================================================
// FUNCTION DECLARATIONS ("Table of Contents" for filesystem.cpp)
// Any function in filesystem.cpp that you want to call from main.cpp
// MUST be listed here.
// ============================================================================


struct DirEntryInfo {
    std::string name;
    bool is_directory;
};
// --- CORE SYSTEM FUNCTIONS ---
void fs_format(const std::string& file_path);
void fs_init(FileSystemInstance& fs_instance, const std::string& file_path);

// --- USER MANAGEMENT ---
bool user_login(FileSystemInstance& fs_instance, const std::string& username, const std::string& password);

// --- DIRECTORY AND FILE OPERATIONS ---
void dir_create(FileSystemInstance& fs_instance, const std::string& path);
void file_create(FileSystemInstance& fs_instance, const std::string& path, const std::string& content);
std::string file_read(FileSystemInstance& fs_instance, const std::string& path);
std::vector<DirEntryInfo> dir_list(FileSystemInstance& fs_instance, const std::string& path);
void file_delete(FileSystemInstance& fs_instance, const std::string& path);
void file_delete(FileSystemInstance& fs_instance, const std::string& path);
// Add this line inside include/filesystem.h
void dir_delete(FileSystemInstance& fs_instance, const std::string& path);
// Add this line inside include/filesystem.h
bool file_exists(FileSystemInstance& fs_instance, const std::string& path);
bool dir_exists(FileSystemInstance& fs_instance, const std::string& path);
void file_rename(FileSystemInstance& fs_instance, const std::string& old_path, const std::string& new_path);
void user_create(FileSystemInstance& fs_instance, const std::string& username, const std::string& password, uint32_t role);
// Add these lines inside include/filesystem.h
void user_delete(FileSystemInstance& fs_instance, const std::string& username);
std::vector<std::string> user_list(FileSystemInstance& fs_instance);
// --- HELPER FUNCTIONS ---
// These are not strictly required to be here, but it's good practice.
int find_free_metadata_entry(FileSystemInstance& fs_instance);
int find_free_block(FileSystemInstance& fs_instance);
int find_entry_by_path(FileSystemInstance& fs_instance, const std::string& path);


#endif // FILESYSTEM_H