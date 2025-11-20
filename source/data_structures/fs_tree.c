#include "fs_tree.h"
#include "ofs_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FSNode* fsnode_create(const char* name, uint8_t is_directory) {
    FSNode* node = (FSNode*)calloc(1, sizeof(FSNode));
    if (!node) return NULL;
    snprintf(node->name, sizeof(node->name), "%s", name);
    node->is_directory = is_directory;
    node->parent = NULL;
    if (is_directory) {
        node->children = ht_create(16);
        if (!node->children) {
            free(node);
            return NULL;
        }
    } else {
        node->children = NULL;
    }
    return node;
}

FSTree* fstree_create() {
    FSTree* tree = (FSTree*)malloc(sizeof(FSTree));
    if (!tree) return NULL;
    tree->root = fsnode_create("/", 1);
    if (!tree->root) { free(tree); return NULL; }
    tree->root->entry_index = 1;
    tree->root->parent = tree->root;
    tree->path_cache = ht_create(1024);
    if (!tree->path_cache) { fsnode_destroy(tree->root); free(tree); return NULL; }
    ht_insert(tree->path_cache, "/", tree->root);
    return tree;
}

int fstree_add_node(FSTree* tree, const char* path, FSNode* node) {
    if (!tree || !path || !node) return -1;
    char parent_path_str[MAX_PATH_LENGTH];
    strncpy(parent_path_str, path, sizeof(parent_path_str));
    parent_path_str[sizeof(parent_path_str) - 1] = '\0';
    char* last_slash = strrchr(parent_path_str, '/');
    if (!last_slash) return -1;
    if (last_slash == parent_path_str && strlen(parent_path_str) > 1) {
        strcpy(parent_path_str, "/");
    } else if (last_slash != parent_path_str) {
        *last_slash = '\0';
    }
    const char* name = last_slash + 1;
    FSNode* parent = fstree_find(tree, parent_path_str);
    if (!parent || !parent->is_directory || !parent->children) {
        printf("ERROR: Could not find valid parent ('%s') for path '%s'!\n", parent_path_str, path);
        return -1;
    }
    node->parent = parent;
    ht_insert(parent->children, name, node);
    ht_insert(tree->path_cache, path, node);
    return 0;
}

FSNode* fstree_find(FSTree* tree, const char* path) {
    if (!tree || !path || !tree->path_cache) return NULL;
    return (FSNode*)ht_get(tree->path_cache, path);
}

void fsnode_destroy(FSNode* node) {
    if (!node) return;
    if (node->children) {
        ht_destroy(node->children, (void (*)(void*))fsnode_destroy);
    }
    free(node);
}

void fstree_destroy(FSTree* tree) {
    if (!tree) return;
    if (tree->path_cache) {
        ht_destroy(tree->path_cache, NULL);
    }
    if (tree->root) {
        fsnode_destroy(tree->root);
    }
    free(tree);
}

FSNode** fstree_list_children(FSNode* dir, size_t* count) {
    *count = 0;
    if (!dir || !dir->is_directory || !dir->children || dir->children->size == 0) return NULL;
    size_t key_count;
    char** keys = ht_get_keys(dir->children, &key_count);
    if(!keys) return NULL;
    FSNode** children = (FSNode**)malloc(key_count * sizeof(FSNode*));
    if (!children) { free(keys); return NULL; }
    for (size_t i = 0; i < key_count; i++) {
        children[i] = (FSNode*)ht_get(dir->children, keys[i]);
    }
    free(keys);
    *count = key_count;
    return children;
}

int fstree_remove(FSTree* tree, const char* path) {
    if (!tree || !path) return -1;
    FSNode* node_to_remove = fstree_find(tree, path);
    if (!node_to_remove || node_to_remove == tree->root) return -1;
    FSNode* parent = node_to_remove->parent;
    if (parent && parent->children) {
        ht_remove(parent->children, node_to_remove->name);
    }
    ht_remove(tree->path_cache, path);
    return 0;
}

// --- FINAL, MEMORY-SAFE VERSION ---
char* fstree_get_path(FSNode* node) {
    if (!node) return NULL;
    if (node->parent == node) return strdup("/");

    // Allocate memory on the heap, not the stack, to prevent corruption
    char* path = (char*)calloc(MAX_PATH_LENGTH, sizeof(char));
    if (!path) return NULL;

    FSNode* current = node;
    while (current != NULL && current->parent != current) {
        // Build the path safely in a temporary buffer
        char temp_path[MAX_PATH_LENGTH];
        snprintf(temp_path, MAX_PATH_LENGTH, "/%s%s", current->name, path);
        // Safely copy it back
        strcpy(path, temp_path);
        current = current->parent;
    }

    if (strlen(path) == 0) {
        strcpy(path, "/");
    }
    
    return path;
}