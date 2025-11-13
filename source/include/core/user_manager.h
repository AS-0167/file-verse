#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "odf_types.hpp"
#include "HashTable.h"

struct SessionInfo;
struct UserInfo;

class user_manager {
private:
    HashTable<UserInfo> *users;               
    std::vector<SessionInfo*> active_sessions;

public:
    explicit user_manager(HashTable<UserInfo>* user_table);
    ~user_manager();

    // User management functions
    int user_login(void** session, const char* username, const char* password);
    int user_logout(void* session);
    int user_create(void* admin_session, const char* username, const char* password, UserRole role);
    int user_delete(void* admin_session, const char* username);
    int user_list(void* admin_session, UserInfo** users_out, int* count);
    int get_session_info(void* session, SessionInfo* info);
    std::string hash_password(const std::string& password);
    int total_users();

    
private:
    bool check_admin(void* session);
    SessionInfo* find_session(void* session);
    
};

#endif // USER_MANAGER_H
