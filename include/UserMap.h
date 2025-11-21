#ifndef USER_MAP_H
#define USER_MAP_H

#include <string>

// Forward declaration of UserInfo.
struct UserInfo;

// A node in the hash table's linked list (for collision handling)
struct UserMapNode {
    std::string key;      // The username
    UserInfo* value;      // Pointer to the UserInfo struct in the main user_table
    UserMapNode* next;
};

// The main hash table structure
struct UserMap {
    UserMapNode** buckets;
    int size;
};

// --- Function Declarations ---
UserMap* user_map_create(int size);
void user_map_destroy(UserMap* map);
void user_map_insert(UserMap* map, const std::string& key, UserInfo* value);
UserInfo* user_map_get(UserMap* map, const std::string& key);

#endif // USER_MAP_H