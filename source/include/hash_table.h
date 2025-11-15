#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <string>

// Forward declaration: Promise the compiler that a struct named "UserInfo" exists.
// This allows us to use pointers to it (UserInfo*) without needing the full definition.
struct UserInfo;

// Represents one entry in our hash table's linked list for collision handling.
struct HashNode {
    std::string key;    // The username
    UserInfo* value;    // A POINTER to the actual UserInfo struct in the main user_table
    HashNode* next;     // Pointer to the next node in case of collision
};

// The main Hash Table structure.
struct HashTable {
    HashNode** buckets; // An array of pointers to HashNode lists
    int size;
};

// --- FUNCTION DECLARATIONS ---
HashTable* hash_table_create(int size);
void hash_table_insert(HashTable* table, const std::string& key, UserInfo* value);
UserInfo* hash_table_get(HashTable* table, const std::string& key);

#endif // HASH_TABLE_H