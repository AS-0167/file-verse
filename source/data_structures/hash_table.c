#include "hash_table.h"
#include "ofs_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Simple hash function (djb2)
static size_t default_hash(const char* str) {
    if (!str) return 0;
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

HashTable* ht_create(size_t capacity) {
    if (capacity == 0) capacity = 1;
    HashTable* ht = (HashTable*)malloc(sizeof(HashTable));
    if (!ht) return NULL;

    ht->capacity = capacity;
    ht->size = 0;
    ht->hash_function = default_hash;

    ht->buckets = (HashNode**)calloc(capacity, sizeof(HashNode*));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    return ht;
}

// --- ht_insert (CLEANED VERSION) ---
int ht_insert(HashTable* ht, const char* key, void* value) {
    if (!ht || !key) {
        return -1;
    }

    size_t index = ht->hash_function(key) % ht->capacity;

    HashNode* node = ht->buckets[index];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            node->value = value;
            return 0;
        }
        node = node->next;
    }

    HashNode* new_node = (HashNode*)malloc(sizeof(HashNode));
    if (!new_node) return -1;

    new_node->key = strdup(key);
    if (!new_node->key) {
        free(new_node);
        return -1;
    }

    new_node->value = value;
    new_node->next = ht->buckets[index];
    ht->buckets[index] = new_node;
    ht->size++;

    return 0;
}

// --- ht_get (CLEANED VERSION) ---
void* ht_get(HashTable* ht, const char* key) {
    if (!ht || !key) {
        return NULL;
    }

    size_t index = ht->hash_function(key) % ht->capacity;

    HashNode* node = ht->buckets[index];
    if (!node) {
        return NULL;
    }

    while (node) {
        if(!node->key) {
            node = node->next;
            continue;
        }

        if (strcmp(node->key, key) == 0) {
            return node->value;
        }
        node = node->next;
    }

    return NULL;
}

// --- Other functions (Unchanged) ---

int ht_remove(HashTable* ht, const char* key) {
    if (!ht || !key) return -1;
    size_t index = ht->hash_function(key) % ht->capacity;
    HashNode* node = ht->buckets[index];
    HashNode* prev = NULL;
    while (node) {
        if (strcmp(node->key, key) == 0) {
            if (prev) {
                prev->next = node->next;
            } else {
                ht->buckets[index] = node->next;
            }
            free(node->key);
            free(node);
            ht->size--;
            return 0;
        }
        prev = node;
        node = node->next;
    }
    return -1;
}

int ht_contains(HashTable* ht, const char* key) {
    return ht_get(ht, key) != NULL;
}

char** ht_get_keys(HashTable* ht, size_t* count) {
    if (!ht || !count || ht->size == 0) {
        if (count) *count = 0;
        return NULL;
    }
    char** keys = (char**)malloc(ht->size * sizeof(char*));
    if (!keys) { *count = 0; return NULL; }
    size_t idx = 0;
    for (size_t i = 0; i < ht->capacity; i++) {
        HashNode* node = ht->buckets[i];
        while (node) {
            if (idx < ht->size) {
                keys[idx++] = node->key;
            }
            node = node->next;
        }
    }
    *count = ht->size;
    return keys;
}

void ht_destroy(HashTable* ht, void (*value_destroyer)(void*)) {
    if (!ht) return;
    for (size_t i = 0; i < ht->capacity; i++) {
        HashNode* node = ht->buckets[i];
        while (node) {
            HashNode* temp = node;
            node = node->next;
            free(temp->key);
            if (value_destroyer) {
                value_destroyer(temp->value);
            }
            free(temp);
        }
    }
    free(ht->buckets);
    free(ht);
}