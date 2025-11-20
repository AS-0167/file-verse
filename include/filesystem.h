#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <vector>
#include <string>
#include "ofs_types.h" // This file now provides all struct definitions

// ============================================================================
// FUNCTION DECLARATIONS ("Table of Contents" for filesystem.cpp)
// ============================================================================

// --- CORE SYSTEM FUNCTIONS ---
void fs_format(const std::string& file_path);
void fs_init(FileSystemInstance& fs_instance, const std::string& file_path);
void fs_shutdown();

// --- USER MANAGEMENT ---
std::string user_login(FileSystemInstance& fs_instance, const std::string& username, const std::string& password);
void user_logout(FileSystemInstance& fs_instance, const std::string& session_id);
void user_create(FileSystemInstance& fs_instance, const std::string& username, const std::string& password, uint32_t role);
void user_delete(FileSystemInstance& fs_instance, const std::string& username);
std::vector<std::string> user_list(FileSystemInstance& fs_instance);
SessionInfo get_session_info(FileSystemInstance& fs_instance, const std::string& session_id);

// --- DIRECTORY AND FILE OPERATIONS ---
void dir_create(FileSystemInstance& fs_instance, const std::string& path);
std::vector<DirEntryInfo> dir_list(FileSystemInstance& fs_instance, const std::string& path);
void dir_delete(FileSystemInstance& fs_instance, const std::string& path);
bool dir_exists(FileSystemInstance& fs_instance, const std::string& path);
void file_create(FileSystemInstance& fs_instance, const std::string& path, const std::string& content);
std::string file_read(FileSystemInstance& fs_instance, const std::string& path);
void file_delete(FileSystemInstance& fs_instance, const std::string& path);
void file_edit(FileSystemInstance& fs_instance, const std::string& path, const std::string& new_content, uint32_t index);
void file_truncate(FileSystemInstance& fs_instance, const std::string& path);
bool file_exists(FileSystemInstance& fs_instance, const std::string& path);
void file_rename(FileSystemInstance& fs_instance, const std::string& old_path, const std::string& new_path);

// --- INFORMATION FUNCTIONS ---
FSStats get_stats(FileSystemInstance& fs_instance);
FileMetadata get_metadata(FileSystemInstance& fs_instance, const std::string& path);
void set_permissions(FileSystemInstance& fs_instance, const std::string& path, uint32_t permissions);
std::string get_error_message(int error_code);

// --- HELPER FUNCTIONS ---
int find_free_metadata_entry(FileSystemInstance& fs_instance);
int find_free_block(FileSystemInstance& fs_instance);
int find_entry_by_path(FileSystemInstance& fs_instance, const std::string& path);

#endif // FILESYSTEM_H