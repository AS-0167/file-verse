#include "../../include/hash_table.h"
#include "../../include/ofs_types.h" // NOW we include the full definition
#include <string>

unsigned int hash_function(const std::string& key, int table_size) {
    unsigned int hash = 0;
    for (char c : key) {
        hash = (hash * 31) + c;
    }
    return hash % table_size;
}

HashTable* hash_table_create(int size) {
    HashTable* table = new HashTable;
    table->size = size;
    table->buckets = new HashNode*[size];
    for (int i = 0; i < size; ++i) {
        table->buckets[i] = nullptr;
    }
    return table;
}

void hash_table_insert(HashTable* table, const std::string& key, UserInfo* value) { // Now takes a UserInfo*
    unsigned int index = hash_function(key, table->size);
    HashNode* new_node = new HashNode;
    new_node->key = key;
    new_node->value = value; // Store the pointer
    new_node->next = nullptr;

    if (table->buckets[index] != nullptr) {
        new_node->next = table->buckets[index];
    }
    table->buckets[index] = new_node;
}

UserInfo* hash_table_get(HashTable* table, const std::string& key) {
    unsigned int index = hash_function(key, table->size);
    HashNode* current = table->buckets[index];
    while (current != nullptr) {
        if (current->key == key) {
            return current->value; // Return the stored pointer
        }
        current = current->next;
    }
    return nullptr;
}