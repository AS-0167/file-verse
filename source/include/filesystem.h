#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <vector>
#include <string>
#include "ofs_types.h"

// Struct for returning directory listing info
struct DirEntryInfo {
    std::string name;
    bool is_directory;
};

// --- CORE SYSTEM FUNCTIONS ---
void fs_format(const std::string& file_path);
void fs_init(FileSystemInstance& fs_instance, const std::string& file_path);

// --- USER MANAGEMENT ---
bool user_login(FileSystemInstance& fs_instance, const std::string& username, const std::string& password);
void user_create(FileSystemInstance& fs_instance, const std::string& username, const std::string& password, uint32_t role);
void user_delete(FileSystemInstance& fs_instance, const std::string& username);
std::vector<std::string> user_list(FileSystemInstance& fs_instance);

// --- DIRECTORY AND FILE OPERATIONS ---
void dir_create(FileSystemInstance& fs_instance, const std::string& path);
void dir_delete(FileSystemInstance& fs_instance, const std::string& path);
std::vector<DirEntryInfo> dir_list(FileSystemInstance& fs_instance, const std::string& path);
bool dir_exists(FileSystemInstance& fs_instance, const std::string& path);

void file_create(FileSystemInstance& fs_instance, const std::string& path, const std::string& content);
std::string file_read(FileSystemInstance& fs_instance, const std::string& path);
void file_delete(FileSystemInstance& fs_instance, const std::string& path);
bool file_exists(FileSystemInstance& fs_instance, const std::string& path);
void file_rename(FileSystemInstance& fs_instance, const std::string& old_path, const std::string& new_path);

#endif // FILESYSTEM_H