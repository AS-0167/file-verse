#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stddef.h>

typedef struct HashNode {
    char* key;
    void* value;
    struct HashNode* next;
} HashNode;

typedef struct HashTable {
    HashNode** buckets;
    size_t capacity;
    size_t size;
    size_t (*hash_function)(const char*);
} HashTable;

HashTable* ht_create(size_t capacity);
int ht_insert(HashTable* ht, const char* key, void* value);
void* ht_get(HashTable* ht, const char* key);
int ht_remove(HashTable* ht, const char* key);
int ht_contains(HashTable* ht, const char* key);
char** ht_get_keys(HashTable* ht, size_t* count);
void ht_destroy(HashTable* ht, void (*value_destroyer)(void*));

#endif // HASH_TABLE_H