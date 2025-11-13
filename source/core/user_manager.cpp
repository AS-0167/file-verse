#include "user_manager.h"
#include <cstring>
#include <ctime>
#include <iostream>
#include <algorithm>
#include <openssl/sha.h>
using namespace std;
// ===================== Constructor & Destructor =====================

user_manager:: user_manager(HashTable<UserInfo>* user_table)
    : users(user_table) {}

user_manager::~user_manager() {
    for (auto s : active_sessions)
        delete s;
}

// ===================== Utility Functions =====================

bool user_manager::check_admin(void* session) {
    SessionInfo* s = find_session(session);
    return s && s->user.role == UserRole::ADMIN;
}

SessionInfo* user_manager::find_session(void* session) {
    for (auto s : active_sessions) {
        if (s == session) return s;
    }
    return nullptr;
}

std::string user_manager::hash_password(const std::string& password) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), password.size(), hash);

    char buf[64];
    for (int i = 0; i < 31; i++)
        sprintf(buf + i*2, "%02x", hash[i]);
    buf[62] = 0;
    return std::string(buf);
}

// ===================== User Management Functions =====================

int user_manager::user_login(void** session, const char* username, const char* password) {
   // cout<< "ENtered";

    if(!session)
        cout <<"Session";
    if(!username)
        cout <<"Username";
    cout << password<<endl;
    if (!session || !username || !password) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    UserInfo* user = users->get(username);
    if (!user || !user->is_active) return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

    std::string hashed = hash_password(password);
    if (hashed != std::string(user->password_hash)) 
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    // Create session
    std::string session_id = username + std::to_string(std::time(nullptr));
    SessionInfo* s = new SessionInfo(session_id, *user, std::time(nullptr));
    active_sessions.push_back(s);
    *session = s;

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int user_manager::user_logout(void* session) {
    SessionInfo* s = find_session(session);
    if (!s) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);

    auto it = find(active_sessions.begin(), active_sessions.end(), s);
    if (it != active_sessions.end()) {
        active_sessions.erase(it);
        delete s;
    }
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int user_manager ::user_create(void* admin_session, const char* username, const char* password, UserRole role) {
    if (!check_admin(admin_session)) return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    if (users->get(username)) return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);

    std::string hashed = hash_password(password);
    UserInfo* new_user = new UserInfo(username, hashed, role, std::time(nullptr));
    if (!users->insert(username, new_user)) {
        delete new_user;
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int user_manager::user_delete(void* admin_session, const char* username) {
    if (!check_admin(admin_session)) return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);

    UserInfo* user = users->get(username);
    if (!user) return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);

    user->is_active = 0; // mark as inactive
    users->remove(username);

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int user_manager::user_list(void* admin_session, UserInfo** users_out, int* count) {
    if (!check_admin(admin_session)) return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    if (!users_out || !count) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);

    std::vector<std::string> keys = users->getAllKeys();
    *count = static_cast<int>(keys.size());
    *users_out = new UserInfo[*count];

    for (int i = 0; i < *count; ++i) {
        UserInfo* u = users->get(keys[i]);
        (*users_out)[i] = *u;
    }

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int user_manager::get_session_info(void* session, SessionInfo* info) {
    if (!session || !info) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);

    SessionInfo* s = find_session(session);
    if (!s) return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);

    *info = *s;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

 int user_manager:: total_users()
 {
    return this->users->get_size() ;
 }