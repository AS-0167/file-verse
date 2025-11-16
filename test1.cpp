#include <iostream>
#include <iomanip>
#include <cstring>
#include <ctime>
#include <queue>
#include "core/fs_core.h"
#include "core/user_manager.h"
#include "core/file_manager.h"
#include "core/dir_manager.h"
#include "core/metadata.h"
#include "odf_types.hpp"

using namespace std;

// Helper function to print file/directory info
void printNodeInfo(FSNode* node, int depth = 0) {
    if (!node || !node->entry) return;
    
    // Print indentation
    for (int i = 0; i < depth; i++) {
        cout << "  ";
    }
    
    // Print name and type
    cout << "|- " << node->entry->name;
    
    if (node->entry->getType() == EntryType::DIRECTORY) {
        cout << " [DIR]";
    } else {
        cout << " [FILE] (" << node->entry->size << " bytes)";
    }
    
    cout << endl;
}

// Recursive traversal
void traverseTree(FSNode* node, int depth = 0) {
    if (!node) return;
    
    printNodeInfo(node, depth);
    
    // Traverse all children if it's a directory
    if (node->entry->getType() == EntryType::DIRECTORY) {
        vector<FSNode*> children = node->getChildren();
        for (auto* child : children) {  // Assuming children is a vector
            traverseTree(child, depth + 1);
        }
    }
}

// Breadth-first traversal (level by level)
void traverseBFS(FSNode* root) {
    if (!root) return;
    
    queue<pair<FSNode*, int>> q;
    q.push({root, 0});
    
    while (!q.empty()) {
        auto [node, depth] = q.front();
        q.pop();
        
        printNodeInfo(node, depth);
        
        if (node->entry->getType() == EntryType::DIRECTORY) {
        vector<FSNode*> children = node->getChildren();
        for (auto* child : children) {
                q.push({child, depth + 1});
            }
        }
    }
}

// Find a specific file/directory by path
FSNode* findByPath(FSNode* root, const string& path) {
    if (!root || path.empty()) return nullptr;
    
    // Split path by '/'
    vector<string> parts;
    stringstream ss(path);
    string part;
    while (getline(ss, part, '/')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    
    FSNode* current = root;
    for (const auto& name : parts) {
        bool found = false;
        if (current->entry->getType() == EntryType::DIRECTORY) {
            vector<FSNode*> children = current->getChildren();
            for (auto* child : children) {
                if (strcmp(child->entry->name, name.c_str()) == 0) {
                    current = child;
                    found = true;
                    break;
                }
            }
        }
        if (!found) return nullptr;
    }
    return current;
}

// Count total files and directories
void countNodes(FSNode* node, int& files, int& dirs) {
    if (!node) return;
    
    if (node->entry->getType() == EntryType::DIRECTORY) {
        dirs++;
        vector<FSNode*> children = node->getChildren();
        for (auto* child : children) {
            countNodes(child, files, dirs);
        }
    } else {
        files++;
    }
}

int main() {
    void* instance = nullptr;
    
    int result = fs_init(&instance, "file.omni", "default_config.txt");
    if (result != 0) {
        cerr << "Failed to initialize filesystem: " << result << endl;
        return 1;
    }
    
    FSInstance* fs = static_cast<FSInstance*>(instance);
    
    cout << "=== FILESYSTEM INFO ===" << endl;
    cout << "Block size: " << fs->header.block_size << " bytes" << endl;
    cout << "Total size: " << fs->header.total_size << " bytes" << endl;
    cout << "Max users: " << fs->header.max_users << endl;
    cout << endl;
    
    // List all users
    cout << "=== USERS ===" << endl;
    // Note: You'll need a way to iterate all users in your HashTable
    // For now, try specific users:
    const char* usernames[] = {"newuser", "n", "testuser", "admin"};
    for (const char* username : usernames) {
        UserInfo* user = fs->users->get(username);
        if (user) {
            cout << "User: " << user->username << endl;
        }
    }
    cout << endl;
    
    // Traverse filesystem tree
    cout << "=== FILESYSTEM TREE ===" << endl;
    if (fs->root) {
        traverseTree(fs->root);
        cout << endl;
        
        // Count files and directories
        int files = 0, dirs = 0;
        countNodes(fs->root, files, dirs);
        cout << "Total: " << dirs << " directories, " << files << " files" << endl;
        cout << endl;
        
        // Find a specific path
        cout << "=== SEARCHING FOR SPECIFIC PATHS ===" << endl;
        FSNode* homeDir = findByPath(fs->root, "home");
        if (homeDir) {
            cout << "Found /home, contains:" << endl;
            vector<FSNode*> children = homeDir->getChildren();
        for (auto* child : children) {
                cout << "  - " << child->entry->name << endl;
            }
        }
        
        FSNode* specificFile = findByPath(fs->root, "home/alice/document.txt");
        if (specificFile) {
            cout << "Found file: " << specificFile->entry->name 
                 << " (" << specificFile->entry->size << " bytes)" << endl;
        }
    } else {
        cout << "Root is null!" << endl;
    }
    
    // Check free space
    cout << endl << "=== FREE SPACE INFO ===" << endl;
    uint64_t totalBlocks = fs->header.total_size / fs->header.block_size;
    uint64_t freeBlocks = 0;
    for (uint64_t i = 0; i < totalBlocks; i++) {
        if (fs->fsm->isFree(i)) {
            freeBlocks++;
        }
    }
    cout << "Free blocks: " << freeBlocks << " / " << totalBlocks << endl;
    cout << "Free space: " << (freeBlocks * fs->header.block_size) << " bytes" << endl;
    
    // Don't forget cleanup!
    // fs_cleanup(instance);
    
    return 0;
}

//g++ -std=c++17 -Isource/include -Isource/include/core source/core/*.cpp source/*.cpp test1.cpp -o test1_ofs -lssl -lcrypto