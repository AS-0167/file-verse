#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <vector>
#include <string>
#include "OFSTypes.h"

void format_filesystem(const std::string& filepath);
void init_filesystem(OFSystem& fs_instance, const std::string& filepath);
void shutdown_filesystem();
std::string login_user(OFSystem& fs_instance, const std::string& username, const std::string& password);
void logout_user(OFSystem& fs_instance, const std::string& session_id);
void create_user(OFSystem& fs_instance, const std::string& username, const std::string& password, uint32_t role);
void delete_user(OFSystem& fs_instance, const std::string& username);
std::vector<std::string> list_all_users(OFSystem& fs_instance);
SessionInfo get_session_details(OFSystem& fs_instance, const std::string& session_id);
void create_directory(OFSystem& fs_instance, const std::string& path);
std::vector<DirEntryInfo> list_directory_contents(OFSystem& fs_instance, const std::string& path);
void remove_directory(OFSystem& fs_instance, const std::string& path);
bool path_is_directory(OFSystem& fs_instance, const std::string& path);
void create_file_with_content(OFSystem& fs_instance, const std::string& path, const std::string& content);
std::string read_file_content(OFSystem& fs_instance, const std::string& path);
void remove_file(OFSystem& fs_instance, const std::string& path);
void edit_file(OFSystem& fs_instance, const std::string& path, const std::string& new_content, uint32_t index);
void truncate_file_content(OFSystem& fs_instance, const std::string& path);
bool path_is_file(OFSystem& fs_instance, const std::string& path);
void rename_path(OFSystem& fs_instance, const std::string& old_path, const std::string& new_path);
FSStats get_fs_stats(OFSystem& fs_instance);
FileMetadata get_path_metadata(OFSystem& fs_instance, const std::string& path);
void set_path_permissions(OFSystem& fs_instance, const std::string& path, uint32_t permissions);
std::string get_error_string(int error_code);

#endif // FILESYSTEM_H