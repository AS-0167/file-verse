#include "fs_tree.h"
#include "ofs_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- fsnode_create (Unchanged) ---
FSNode* fsnode_create(const char* name, uint8_t is_directory) {
    FSNode* node = (FSNode*)calloc(1, sizeof(FSNode));
    if (!node) return NULL;

    strncpy(node->name, name, 11);
    node->name[11] = '\0';
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

// --- fstree_create (Unchanged) ---
FSTree* fstree_create() {
    FSTree* tree = (FSTree*)malloc(sizeof(FSTree));
    if (!tree) return NULL;

    tree->root = fsnode_create("/", 1);
    if (!tree->root) { free(tree); return NULL; }
    tree->root->entry_index = 1;

    tree->path_cache = ht_create(1024);
    if (!tree->path_cache) { fsnode_destroy(tree->root); free(tree); return NULL; }

    ht_insert(tree->path_cache, "/", tree->root);
    return tree;
}

// --- fstree_add_node (CLEANED VERSION) ---
int fstree_add_node(FSTree* tree, const char* path, FSNode* node) {
    if (!tree) {
        // In a real-world scenario, you might log this to a file instead of exiting.
        // For this project, a fatal error is acceptable for an unrecoverable state.
        fprintf(stderr, "FATAL: fstree_add_node received a NULL tree pointer!\n");
        exit(1);
    }

    char parent_path[MAX_PATH_LENGTH];
    strncpy(parent_path, path, sizeof(parent_path) - 1);
    parent_path[MAX_PATH_LENGTH - 1] = '\0';
    char* last_slash = strrchr(parent_path, '/');
    if (!last_slash) return -1;

    if (last_slash == parent_path) {
        strcpy(parent_path, "/");
    } else {
        *last_slash = '\0';
    }

    const char* name = last_slash + 1;
    FSNode* parent = fstree_find(tree, parent_path);

    if (!parent || !parent->is_directory || !parent->children) {
        fprintf(stderr, "FATAL: Could not find a valid parent directory for path '%s'!\n", path);
        exit(1);
    }

    node->parent = parent;
    ht_insert(parent->children, name, node);

    if (!tree->path_cache) {
        fprintf(stderr, "FATAL: tree->path_cache is NULL during add operation!\n");
        exit(1);
    }
    ht_insert(tree->path_cache, path, node);

    return 0;
}

// --- Other functions (Unchanged) ---

FSNode* fstree_find(FSTree* tree, const char* path) {
    if (!tree || !path || !tree->path_cache) return NULL;
    return (FSNode*)ht_get(tree->path_cache, path);
}

void fsnode_destroy(FSNode* node) {
    if (!node) return;
    if (node->children) {
        ht_destroy(node->children, (void(*)(void*))fsnode_destroy);
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
    if (!dir || !dir->is_directory || !dir->children || dir->children->size == 0) {
        return NULL;
    }

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

char* fstree_get_path(FSNode* node) {
    if (!node) return NULL;
    // Note: This is a simplified function. A full implementation would
    // traverse up the parent pointers to construct the full path.
    return strdup(node->name);
}