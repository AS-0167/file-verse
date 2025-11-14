#ifndef FS_TREE_H
#define FS_TREE_H

#include "hash_table.h"
#include <stdint.h>

typedef struct FSNode {
    char name[12];
    uint8_t is_directory;
    uint32_t entry_index;
    uint32_t start_block;
    uint64_t size;
    uint32_t owner_id;
    uint32_t permissions;
    uint64_t created_time;
    uint64_t modified_time;
    struct FSNode* parent;
    HashTable* children;
} FSNode;

typedef struct FSTree {
    FSNode* root;
    HashTable* path_cache;
} FSTree;

FSTree* fstree_create();
int fstree_add_node(FSTree* tree, const char* path, FSNode* node);
FSNode* fstree_find(FSTree* tree, const char* path);
int fstree_remove(FSTree* tree, const char* path);
FSNode** fstree_list_children(FSNode* dir, size_t* count);
char* fstree_get_path(FSNode* node);
void fstree_destroy(FSTree* tree);
FSNode* fsnode_create(const char* name, uint8_t is_directory);
void fsnode_destroy(FSNode* node);

#endif // FS_TREE_H