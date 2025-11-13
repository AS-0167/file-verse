#ifndef OFS_CORE_HPP
#define OFS_CORE_HPP

#include "odf_types.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <memory>

// ============================================================================
// CUSTOM DATA STRUCTURES (Since we can only use vector)
// ============================================================================

/**
 * Simple Binary Search Tree Node for O(log n) username lookup
 */
template<typename T>
struct BSTNode {
    std::string key;
    T data;
    BSTNode* left;
    BSTNode* right;
    
    BSTNode(const std::string& k, const T& d) 
        : key(k), data(d), left(nullptr), right(nullptr) {}
};

/**
 * Binary Search Tree for efficient user lookup
 */
template<typename T>
class BST {
private:
    BSTNode<T>* root;
    
    BSTNode<T>* insert(BSTNode<T>* node, const std::string& key, const T& data) {
        if (!node) return new BSTNode<T>(key, data);
        if (key < node->key) node->left = insert(node->left, key, data);
        else if (key > node->key) node->right = insert(node->right, key, data);
        else node->data = data; // Update if exists
        return node;
    }
    
    BSTNode<T>* search(BSTNode<T>* node, const std::string& key) {
        if (!node || node->key == key) return node;
        if (key < node->key) return search(node->left, key);
        return search(node->right, key);
    }
    
    void inorder(BSTNode<T>* node, std::vector<T>& result) {
        if (!node) return;
        inorder(node->left, result);
        result.push_back(node->data);
        inorder(node->right, result);
    }
    
    BSTNode<T>* findMin(BSTNode<T>* node) {
        while (node && node->left) node = node->left;
        return node;
    }
    
    BSTNode<T>* remove(BSTNode<T>* node, const std::string& key) {
        if (!node) return nullptr;
        
        if (key < node->key) {
            node->left = remove(node->left, key);
        } else if (key > node->key) {
            node->right = remove(node->right, key);
        } else {
            // Node found
            if (!node->left) {
                BSTNode<T>* temp = node->right;
                delete node;
                return temp;
            } else if (!node->right) {
                BSTNode<T>* temp = node->left;
                delete node;
                return temp;
            }
            
            // Two children
            BSTNode<T>* temp = findMin(node->right);
            node->key = temp->key;
            node->data = temp->data;
            node->right = remove(node->right, temp->key);
        }
        return node;
    }
    
    void cleanup(BSTNode<T>* node) {
        if (!node) return;
        cleanup(node->left);
        cleanup(node->right);
        delete node;
    }
    
public:
    BST() : root(nullptr) {}
    ~BST() { cleanup(root); }
    
    void insert(const std::string& key, const T& data) {
        root = insert(root, key, data);
    }
    
    T* find(const std::string& key) {
        BSTNode<T>* node = search(root, key);
        return node ? &node->data : nullptr;
    }
    
    void remove(const std::string& key) {
        root = remove(root, key);
    }
    
    std::vector<T> getAllValues() {
        std::vector<T> result;
        inorder(root, result);
        return result;
    }
};

// ============================================================================
// FILE SYSTEM STRUCTURES
// ============================================================================

/**
 * Configuration structure parsed from .uconf file
 */
struct FSConfig {
    uint64_t total_size;
    uint64_t header_size;
    uint64_t block_size;
    uint32_t max_files;
    uint32_t max_filename_length;
    uint32_t max_users;
    std::string admin_username;
    std::string admin_password;
    bool require_auth;
    uint32_t port;
    uint32_t max_connections;
    uint32_t queue_timeout;
    
    FSConfig() : total_size(104857600), header_size(512), block_size(4096),
                 max_files(1000), max_filename_length(10), max_users(50),
                 admin_username("admin"), admin_password("admin123"),
                 require_auth(true), port(8080), max_connections(20),
                 queue_timeout(30) {}
};

/**
 * Metadata entry for files/directories (72 bytes fixed size)
 */
struct MetadataEntry {
    uint8_t is_valid;           // 0 = in use, 1 = free
    uint8_t is_directory;       // 0 = file, 1 = directory
    uint32_t parent_index;      // Parent directory entry index (0 for root)
    char name[12];              // Short name (10 chars + null + padding)
    uint32_t start_block;       // First block index (0 if empty)
    uint64_t total_size;        // Logical size in bytes
    uint32_t owner_id;          // User ID of owner
    uint32_t permissions;       // UNIX-style permissions
    uint64_t created_time;      // Creation timestamp
    uint64_t modified_time;     // Modification timestamp
    uint32_t inode;             // Unique file identifier
    uint8_t reserved[8];        // Reserved for future use
    
    MetadataEntry() {
        std::memset(this, 0, sizeof(MetadataEntry));
        is_valid = 1; // Mark as free initially
    }
};  // Total: 72 bytes

/**
 * Block header for linked list of blocks
 */
struct BlockHeader {
    uint32_t next_block;        // Next block index (0 = last block)
    uint8_t data[1];            // Flexible array member for content
    
    static constexpr size_t HEADER_SIZE = sizeof(uint32_t);
};

/**
 * Session structure for user sessions
 */
struct Session {
    std::string session_id;
    uint32_t user_id;
    std::string username;
    UserRole role;
    uint64_t login_time;
    uint64_t last_activity;
    uint32_t operations_count;
    
    Session() : user_id(0), role(UserRole::NORMAL), login_time(0),
                last_activity(0), operations_count(0) {}
};

/**
 * Path component for efficient path traversal
 */
struct PathNode {
    std::string name;
    uint32_t entry_index;
    bool is_directory;
    std::vector<uint32_t> children; // Entry indices of children
    
    PathNode(const std::string& n, uint32_t idx, bool is_dir)
        : name(n), entry_index(idx), is_directory(is_dir) {}
};

// ============================================================================
// MAIN FILE SYSTEM CLASS
// ============================================================================

class OFSCore {
private:
    // Configuration
    FSConfig config;
    std::string omni_path;
    
    // File handle
    std::fstream omni_file;
    
    // In-memory structures
    OMNIHeader header;
    BST<UserInfo> users;                    // O(log n) user lookup by username
    std::vector<Session*> sessions;         // Active sessions
    std::vector<MetadataEntry> metadata;    // All file/directory metadata
    std::vector<bool> free_blocks;          // Bitmap for free space tracking
    BST<uint32_t> path_index;               // O(log n) path to entry_index mapping
    
    // Statistics
    uint32_t total_files;
    uint32_t total_directories;
    uint32_t next_inode;
    uint32_t next_user_id;
    
    // Private encoding table for file content
    uint8_t encode_table[256];
    uint8_t decode_table[256];
    
    // Helper functions
    bool parseConfig(const std::string& config_path);
    void initializeEncodingTable();
    std::string hashPassword(const std::string& password);
    std::string generateSessionId();
    uint64_t getCurrentTimestamp();
    
    // Disk operations
    bool writeHeader();
    bool readHeader();
    bool writeUserTable();
    bool readUserTable();
    bool writeMetadataEntry(uint32_t index, const MetadataEntry& entry);
    bool readMetadataEntry(uint32_t index, MetadataEntry& entry);
    bool writeBlock(uint32_t block_index, const uint8_t* data, size_t size);
    bool readBlock(uint32_t block_index, uint8_t* data, size_t size);
    
    // Block management
    uint32_t allocateBlock();
    void freeBlock(uint32_t block_index);
    std::vector<uint32_t> allocateBlocks(uint32_t count);
    void freeBlockChain(uint32_t start_block);
    
    // Metadata management
    uint32_t allocateMetadataEntry();
    void freeMetadataEntry(uint32_t index);
    
    // Path operations
    std::vector<std::string> splitPath(const std::string& path);
    uint32_t traversePath(const std::string& path, bool& is_directory);
    std::string getFullPath(uint32_t entry_index);
    bool addToDirectory(uint32_t dir_index, uint32_t child_index);
    bool removeFromDirectory(uint32_t dir_index, uint32_t child_index);
    
    // Permission checks
    bool checkPermission(Session* session, uint32_t entry_index, bool write_access);
    Session* getSession(void* session_ptr);
    
    // Encoding/Decoding
    void encodeData(uint8_t* data, size_t size);
    void decodeData(uint8_t* data, size_t size);

public:
    OFSCore();
    ~OFSCore();
    
    // Core system functions
    int initialize(const std::string& omni_path, const std::string& config_path);
    void shutdown();
    int format(const std::string& omni_path, const std::string& config_path);
    
    // User management
    int userLogin(void** session, const char* username, const char* password);
    int userLogout(void* session);
    int userCreate(void* admin_session, const char* username, const char* password, UserRole role);
    int userDelete(void* admin_session, const char* username);
    int userList(void* admin_session, UserInfo** users_out, int* count);
    int getSessionInfo(void* session, SessionInfo* info);
    
    // File operations
    int fileCreate(void* session, const char* path, const char* data, size_t size);
    int fileRead(void* session, const char* path, char** buffer, size_t* size);
    int fileEdit(void* session, const char* path, const char* data, size_t size, uint32_t index);
    int fileDelete(void* session, const char* path);
    int fileTruncate(void* session, const char* path);
    int fileExists(void* session, const char* path);
    int fileRename(void* session, const char* old_path, const char* new_path);
    
    // Directory operations
    int dirCreate(void* session, const char* path);
    int dirList(void* session, const char* path, FileEntry** entries, int* count);
    int dirDelete(void* session, const char* path);
    int dirExists(void* session, const char* path);
    
    // Information functions
    int getMetadata(void* session, const char* path, FileMetadata* meta);
    int setPermissions(void* session, const char* path, uint32_t permissions);
    int getStats(void* session, FSStats* stats);
    void freeBuffer(void* buffer);
    const char* getErrorMessage(int error_code);
};

#endif // OFS_CORE_HPP