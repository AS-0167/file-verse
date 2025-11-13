#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <string> // We need this for std::string

// This is the FORWARD DECLARATION.
// We are promising the compiler that a struct named "UserInfo" exists somewhere.
// This is enough for it to understand pointers like "UserInfo*".
struct UserInfo;

// This represents one entry in our hash table.
struct HashNode {
    std::string key;    // The username
    UserInfo* value;    // CHANGE: We now store a POINTER to the UserInfo
    HashNode* next;     // Pointer to the next user in case of collision
};

// This is the main Hash Table structure.
struct HashTable {
    HashNode** buckets;
    int size;
};

// --- FUNCTION DECLARATIONS ---
HashTable* hash_table_create(int size);

// CHANGE: We now pass a POINTER to the UserInfo
void hash_table_insert(HashTable* table, const std::string& key, UserInfo* value);

UserInfo* hash_table_get(HashTable* table, const std::string& key);

#endif // HASH_TABLE_H