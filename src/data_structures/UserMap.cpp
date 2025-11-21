#include "../../include/UserMap.h"     // INCLUDE THIS FIRST!
#include "../../include/OFSTypes.h"   // Now include this for the UserInfo definition
#include <string>
#include <functional>

// Hash function to determine the bucket index for a username
unsigned int hash_key(const std::string& key, int table_size) {
    return std::hash<std::string>{}(key) % table_size;
}

UserMap* user_map_create(int size) {
    UserMap* map = new UserMap;
    map->size = size;
    map->buckets = new UserMapNode*[size](); // Initialize all buckets to nullptr
    return map;
}

void user_map_destroy(UserMap* map) {
    if (!map) return;
    for (int i = 0; i < map->size; ++i) {
        UserMapNode* current = map->buckets[i];
        while (current != nullptr) {
            UserMapNode* to_delete = current;
            current = current->next;
            delete to_delete;
        }
    }
    delete[] map->buckets;
    delete map;
}

void user_map_insert(UserMap* map, const std::string& key, UserInfo* value) {
    unsigned int index = hash_key(key, map->size);
    UserMapNode* new_node = new UserMapNode;
    new_node->key = key;
    new_node->value = value;
    new_node->next = map->buckets[index]; // New node points to the old head of the list
    map->buckets[index] = new_node;   // The new node is now the head
}

UserInfo* user_map_get(UserMap* map, const std::string& key) {
    unsigned int index = hash_key(key, map->size);
    UserMapNode* current = map->buckets[index];
    while (current != nullptr) {
        if (current->key == key) {
            return current->value; // Found the user
        }
        current = current->next;
    }
    return nullptr; // User not found
}