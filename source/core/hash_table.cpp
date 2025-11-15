#include "include/hash_table.h"
#include "include/ofs_types.h"

// ... the rest of the file stays the same ...
// A simple hash function for strings.
unsigned int hash_function(const std::string& key, int table_size) {
    unsigned int hash = 0;
    for (char c : key) {
        hash = (hash * 31) + c; // A common and effective hashing algorithm
    }
    return hash % table_size;
}

HashTable* hash_table_create(int size) {
    HashTable* table = new HashTable;
    table->size = size;
    table->buckets = new HashNode*[size](); // Initialize all buckets to nullptr
    return table;
}

void hash_table_insert(HashTable* table, const std::string& key, UserInfo* value) {
    unsigned int index = hash_function(key, table->size);
    HashNode* new_node = new HashNode;
    new_node->key = key;
    new_node->value = value;
    new_node->next = table->buckets[index]; // New node points to the old head of the list
    table->buckets[index] = new_node;      // The new node is now the head
}

UserInfo* hash_table_get(HashTable* table, const std::string& key) {
    unsigned int index = hash_function(key, table->size);
    HashNode* current = table->buckets[index];
    while (current != nullptr) {
        if (current->key == key) {
            return current->value; // Found it! Return the pointer to the UserInfo.
        }
        current = current->next;
    }
    return nullptr; // Not found
}