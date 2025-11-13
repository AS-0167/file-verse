#include "ofs_core.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <iomanip>

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

OFSCore::OFSCore()
    : total_files(0), total_directories(0), next_inode(1), next_user_id(1)
{
    initializeEncodingTable();
}

OFSCore::~OFSCore()
{
    shutdown();
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void OFSCore::initializeEncodingTable()
{
    // Simple XOR-based encoding with a key
    const uint8_t key = 0xAB;
    for (int i = 0; i < 256; i++)
    {
        encode_table[i] = (i ^ key) & 0xFF;
        decode_table[encode_table[i]] = i;
    }
}

std::string OFSCore::hashPassword(const std::string &password)
{
    // Simple hash function (in production, use SHA-256)
    uint64_t hash = 5381;
    for (char c : password)
    {
        hash = ((hash << 5) + hash) + c;
    }

    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

std::string OFSCore::generateSessionId()
{
    static uint64_t session_counter = 0;
    std::stringstream ss;
    ss << "SESSION_" << getCurrentTimestamp() << "_" << (++session_counter);
    return ss.str();
}

uint64_t OFSCore::getCurrentTimestamp()
{
    return static_cast<uint64_t>(std::time(nullptr));
}

bool OFSCore::parseConfig(const std::string &config_path)
{
    std::ifstream file(config_path);
    if (!file.is_open())
    {
        std::cerr << "Failed to open config file: " << config_path << std::endl;
        return false;
    }

    std::string line, section;
    while (std::getline(file, line))
    {
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos)
        {
            line = line.substr(0, comment_pos);
        }

        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty())
            continue;

        // Section header
        if (line[0] == '[' && line[line.length() - 1] == ']')
        {
            section = line.substr(1, line.length() - 2);
            continue;
        }

        // Key-value pair
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos)
            continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t\""));
        value.erase(value.find_last_not_of(" \t\"") + 1);

        // Parse values based on section
        if (section == "filesystem")
        {
            if (key == "total_size")
                config.total_size = std::stoull(value);
            else if (key == "header_size")
                config.header_size = std::stoull(value);
            else if (key == "block_size")
                config.block_size = std::stoull(value);
            else if (key == "max_files")
                config.max_files = std::stoul(value);
            else if (key == "max_filename_length")
                config.max_filename_length = std::stoul(value);
        }
        else if (section == "security")
        {
            if (key == "max_users")
                config.max_users = std::stoul(value);
            else if (key == "admin_username")
                config.admin_username = value;
            else if (key == "admin_password")
                config.admin_password = value;
            else if (key == "require_auth")
                config.require_auth = (value == "true");
        }
        else if (section == "server")
        {
            if (key == "port")
                config.port = std::stoul(value);
            else if (key == "max_connections")
                config.max_connections = std::stoul(value);
            else if (key == "queue_timeout")
                config.queue_timeout = std::stoul(value);
        }
    }

    file.close();
    return true;
}

std::vector<std::string> OFSCore::splitPath(const std::string &path)
{
    std::vector<std::string> components;
    std::stringstream ss(path);
    std::string component;

    while (std::getline(ss, component, '/'))
    {
        if (!component.empty() && component != ".")
        {
            components.push_back(component);
        }
    }

    return components;
}

// ============================================================================
// DISK I/O OPERATIONS
// ============================================================================

bool OFSCore::writeHeader()
{
    omni_file.seekp(0, std::ios::beg);
    omni_file.write(reinterpret_cast<const char *>(&header), sizeof(OMNIHeader));
    return omni_file.good();
}

bool OFSCore::readHeader()
{
    omni_file.seekg(0, std::ios::beg);
    omni_file.read(reinterpret_cast<char *>(&header), sizeof(OMNIHeader));
    return omni_file.good();
}

bool OFSCore::writeUserTable()
{
    std::vector<UserInfo> user_list = users.getAllValues();

    omni_file.seekp(header.user_table_offset, std::ios::beg);

    // Write user count
    uint32_t count = static_cast<uint32_t>(user_list.size());
    omni_file.write(reinterpret_cast<const char *>(&count), sizeof(uint32_t));

    // Write users
    for (const auto &user : user_list)
    {
        omni_file.write(reinterpret_cast<const char *>(&user), sizeof(UserInfo));
    }

    return omni_file.good();
}

bool OFSCore::readUserTable() {
    omni_file.seekg(header.user_table_offset);
    
    // Read user count
    uint32_t count;
    omni_file.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    
    std::cout << "Reading " << count << " users from user table" << std::endl;
    
    // Read each user
    for (uint32_t i = 0; i < count; i++) {
        UserInfo user;
        omni_file.read(reinterpret_cast<char*>(&user), sizeof(UserInfo));
        
        std::cout << "  User " << i << ": username='" << user.username 
                  << "' active=" << (int)user.is_active << std::endl;
        
        // Only add if:
        // 1. Is active (is_active == 1)
        // 2. Username is not empty (username[0] != '\0')
        // 3. Username is not just whitespace
        if (user.is_active == 1 && user.username[0] != '\0' && user.username[0] != ' ') {
            users.insert(std::string(user.username), user);
            std::cout << "    → Added to BST" << std::endl;
        } else {
            std::cout << "    → Skipped (inactive or empty)" << std::endl;
        }
    }
    
    return omni_file.good();
}

bool OFSCore::writeMetadataEntry(uint32_t index, const MetadataEntry &entry)
{
    uint64_t offset = header.user_table_offset +
                      sizeof(uint32_t) + // User count
                      (config.max_users * sizeof(UserInfo)) +
                      (index * sizeof(MetadataEntry));

    omni_file.seekp(offset, std::ios::beg);
    omni_file.write(reinterpret_cast<const char *>(&entry), sizeof(MetadataEntry));
    return omni_file.good();
}

bool OFSCore::readMetadataEntry(uint32_t index, MetadataEntry &entry)
{
    uint64_t offset = header.user_table_offset +
                      sizeof(uint32_t) +
                      (config.max_users * sizeof(UserInfo)) +
                      (index * sizeof(MetadataEntry));

    omni_file.seekg(offset, std::ios::beg);
    omni_file.read(reinterpret_cast<char *>(&entry), sizeof(MetadataEntry));
    return omni_file.good();
}

bool OFSCore::writeBlock(uint32_t block_index, const uint8_t *data, size_t size)
{
    uint64_t content_area_offset = header.user_table_offset +
                                   sizeof(uint32_t) +
                                   (config.max_users * sizeof(UserInfo)) +
                                   (config.max_files * sizeof(MetadataEntry)) +
                                   ((config.total_size / config.block_size) * sizeof(bool));

    uint64_t offset = content_area_offset + (block_index * config.block_size);

    omni_file.seekp(offset, std::ios::beg);
    omni_file.write(reinterpret_cast<const char *>(data), size);
    return omni_file.good();
}

bool OFSCore::readBlock(uint32_t block_index, uint8_t *data, size_t size)
{
    uint64_t content_area_offset = header.user_table_offset +
                                   sizeof(uint32_t) +
                                   (config.max_users * sizeof(UserInfo)) +
                                   (config.max_files * sizeof(MetadataEntry)) +
                                   ((config.total_size / config.block_size) * sizeof(bool));

    uint64_t offset = content_area_offset + (block_index * config.block_size);

    omni_file.seekg(offset, std::ios::beg);
    omni_file.read(reinterpret_cast<char *>(data), size);
    return omni_file.good();
}

// ============================================================================
// ENCODING/DECODING
// ============================================================================

void OFSCore::encodeData(uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        data[i] = encode_table[data[i]];
    }
}

void OFSCore::decodeData(uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        data[i] = decode_table[data[i]];
    }
}

// ============================================================================
// BLOCK MANAGEMENT
// ============================================================================

uint32_t OFSCore::allocateBlock()
{
    for (size_t i = 1; i < free_blocks.size(); i++)
    {
        if (free_blocks[i])
        {
            free_blocks[i] = false;
            return static_cast<uint32_t>(i);
        }
    }
    return 0; // No free blocks
}

void OFSCore::freeBlock(uint32_t block_index)
{
    if (block_index > 0 && block_index < free_blocks.size())
    {
        free_blocks[block_index] = true;
    }
}

std::vector<uint32_t> OFSCore::allocateBlocks(uint32_t count)
{
    std::vector<uint32_t> allocated;

    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t block = allocateBlock();
        if (block == 0)
        {
            // Rollback
            for (uint32_t b : allocated)
            {
                freeBlock(b);
            }
            return {};
        }
        allocated.push_back(block);
    }

    return allocated;
}

void OFSCore::freeBlockChain(uint32_t start_block)
{
    uint32_t current = start_block;

    while (current != 0)
    {
        uint8_t *block_data = new uint8_t[config.block_size];
        readBlock(current, block_data, config.block_size);

        uint32_t next = *reinterpret_cast<uint32_t *>(block_data);
        freeBlock(current);

        delete[] block_data;
        current = next;
    }
}

// Continued in next artifact due to length...
// Continuation of ofs_core.cpp

// ============================================================================
// METADATA MANAGEMENT
// ============================================================================

uint32_t OFSCore::allocateMetadataEntry()
{
    for (size_t i = 1; i < metadata.size(); i++)
    {
        if (metadata[i].is_valid == 1)
        {                             // Free entry
            metadata[i].is_valid = 0; // Mark as in use
            return static_cast<uint32_t>(i);
        }
    }
    return 0;
}

void OFSCore::freeMetadataEntry(uint32_t index)
{
    if (index > 0 && index < metadata.size())
    {
        metadata[index].is_valid = 1; // Mark as free
        writeMetadataEntry(index, metadata[index]);
    }
}

// ============================================================================
// PATH OPERATIONS
// ============================================================================

uint32_t OFSCore::traversePath(const std::string &path, bool &is_directory)
{
    if (path == "/" || path.empty())
    {
        is_directory = true;
        return 1; // Root directory is always at index 1
    }

    // Check cache first
    uint32_t *cached = path_index.find(path);
    if (cached)
    {
        is_directory = metadata[*cached].is_directory;
        return *cached;
    }

    std::vector<std::string> components = splitPath(path);
    uint32_t current_index = 1; // Start from root

    for (size_t i = 0; i < components.size(); i++)
    {
        const std::string &component = components[i];
        bool found = false;

        // Read directory content
        MetadataEntry &current = metadata[current_index];
        if (!current.is_directory)
        {
            return 0; // Not a directory
        }

        if (current.start_block == 0)
        {
            return 0; // Empty directory
        }

        // Read child indices from directory blocks
        uint32_t block = current.start_block;
        while (block != 0)
        {
            uint8_t *block_data = new uint8_t[config.block_size];
            readBlock(block, block_data, config.block_size);

            uint32_t next_block = *reinterpret_cast<uint32_t *>(block_data);
            uint32_t *child_indices = reinterpret_cast<uint32_t *>(block_data + sizeof(uint32_t));
            size_t num_children = (config.block_size - sizeof(uint32_t)) / sizeof(uint32_t);

            for (size_t j = 0; j < num_children; j++)
            {
                uint32_t child_idx = child_indices[j];
                if (child_idx == 0)
                    break;

                if (child_idx < metadata.size() && metadata[child_idx].is_valid == 0)
                {
                    if (std::string(metadata[child_idx].name) == component)
                    {
                        current_index = child_idx;
                        found = true;
                        break;
                    }
                }
            }

            delete[] block_data;
            if (found)
                break;
            block = next_block;
        }

        if (!found)
        {
            return 0; // Component not found
        }
    }

    // Cache the result
    path_index.insert(path, current_index);
    is_directory = metadata[current_index].is_directory;
    return current_index;
}

std::string OFSCore::getFullPath(uint32_t entry_index)
{
    if (entry_index == 1)
        return "/";

    std::vector<std::string> components;
    uint32_t current = entry_index;

    while (current != 1 && current != 0)
    {
        components.push_back(std::string(metadata[current].name));
        current = metadata[current].parent_index;
    }

    std::string path = "/";
    for (auto it = components.rbegin(); it != components.rend(); ++it)
    {
        path += *it;
        if (it + 1 != components.rend())
            path += "/";
    }

    return path;
}

bool OFSCore::addToDirectory(uint32_t dir_index, uint32_t child_index)
{
    MetadataEntry &dir = metadata[dir_index];

    if (dir.start_block == 0)
    {
        // Allocate first block for directory
        dir.start_block = allocateBlock();
        if (dir.start_block == 0)
            return false;

        uint8_t *block_data = new uint8_t[config.block_size];
        std::memset(block_data, 0, config.block_size);

        *reinterpret_cast<uint32_t *>(block_data) = 0; // No next block
        *reinterpret_cast<uint32_t *>(block_data + sizeof(uint32_t)) = child_index;

        writeBlock(dir.start_block, block_data, config.block_size);
        delete[] block_data;

        writeMetadataEntry(dir_index, dir);
        return true;
    }

    // Find a free slot in existing blocks or add new block
    uint32_t block = dir.start_block;
    uint32_t prev_block = 0;

    while (block != 0)
    {
        uint8_t *block_data = new uint8_t[config.block_size];
        readBlock(block, block_data, config.block_size);

        uint32_t next_block = *reinterpret_cast<uint32_t *>(block_data);
        uint32_t *children = reinterpret_cast<uint32_t *>(block_data + sizeof(uint32_t));
        size_t max_children = (config.block_size - sizeof(uint32_t)) / sizeof(uint32_t);

        // Find free slot
        for (size_t i = 0; i < max_children; i++)
        {
            if (children[i] == 0)
            {
                children[i] = child_index;
                writeBlock(block, block_data, config.block_size);
                delete[] block_data;
                return true;
            }
        }

        delete[] block_data;
        prev_block = block;
        block = next_block;
    }

    // Need to allocate new block
    uint32_t new_block = allocateBlock();
    if (new_block == 0)
        return false;

    // Update previous block to point to new block
    uint8_t *prev_data = new uint8_t[config.block_size];
    readBlock(prev_block, prev_data, config.block_size);
    *reinterpret_cast<uint32_t *>(prev_data) = new_block;
    writeBlock(prev_block, prev_data, config.block_size);
    delete[] prev_data;

    // Initialize new block
    uint8_t *new_data = new uint8_t[config.block_size];
    std::memset(new_data, 0, config.block_size);
    *reinterpret_cast<uint32_t *>(new_data) = 0;
    *reinterpret_cast<uint32_t *>(new_data + sizeof(uint32_t)) = child_index;
    writeBlock(new_block, new_data, config.block_size);
    delete[] new_data;

    return true;
}

bool OFSCore::removeFromDirectory(uint32_t dir_index, uint32_t child_index)
{
    MetadataEntry &dir = metadata[dir_index];
    if (dir.start_block == 0)
        return false;

    uint32_t block = dir.start_block;
    while (block != 0)
    {
        uint8_t *block_data = new uint8_t[config.block_size];
        readBlock(block, block_data, config.block_size);

        uint32_t next_block = *reinterpret_cast<uint32_t *>(block_data);
        uint32_t *children = reinterpret_cast<uint32_t *>(block_data + sizeof(uint32_t));
        size_t max_children = (config.block_size - sizeof(uint32_t)) / sizeof(uint32_t);

        for (size_t i = 0; i < max_children; i++)
        {
            if (children[i] == child_index)
            {
                children[i] = 0;
                writeBlock(block, block_data, config.block_size);
                delete[] block_data;
                return true;
            }
        }

        delete[] block_data;
        block = next_block;
    }

    return false;
}

// ============================================================================
// PERMISSION CHECKING
// ============================================================================

Session *OFSCore::getSession(void *session_ptr)
{
    return static_cast<Session *>(session_ptr);
}

bool OFSCore::checkPermission(Session *session, uint32_t entry_index, bool write_access)
{
    if (!session)
        return false;
    if (session->role == UserRole::ADMIN)
        return true;

    MetadataEntry &entry = metadata[entry_index];

    // Owner check
    if (entry.owner_id == session->user_id)
    {
        if (write_access)
        {
            return (entry.permissions & 0200) != 0; // Owner write
        }
        else
        {
            return (entry.permissions & 0400) != 0; // Owner read
        }
    }

    // Others check
    if (write_access)
    {
        return (entry.permissions & 0002) != 0; // Others write
    }
    else
    {
        return (entry.permissions & 0004) != 0; // Others read
    }
}

// ============================================================================
// CORE SYSTEM FUNCTIONS
// ============================================================================

int OFSCore::initialize(const std::string& path, const std::string& config_path) {
    omni_path = path;
    
    // Parse configuration
    if (!parseConfig(config_path)) {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_CONFIG);
    }
    
    // Open .omni file
    omni_file.open(omni_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!omni_file.is_open()) {
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    // Read header
    if (!readHeader()) {
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    // Validate magic number
    if (std::string(header.magic, 8) != "OMNIFS01") {
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    // Load users
    if (!readUserTable()) {
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }
    
    // Initialize counters
    total_files = 0;
    total_directories = 0;
    next_inode = 2;  // Start at 2 (1 is reserved for root)
    
    // Load metadata - resize first
    metadata.resize(config.max_files + 1);
    
    // Read all metadata entries
    for (uint32_t i = 0; i <= config.max_files; i++) {
        readMetadataEntry(i, metadata[i]);
        
        // Entry 1 is always root directory
        if (i == 1) {
            if (metadata[1].is_valid == 0) {
                total_directories++;
                path_index.insert("/", 1);
            }
            continue;
        }
        
        // Count other valid entries (is_valid == 0 means IN USE)
        if (i > 1 && metadata[i].is_valid == 0 && metadata[i].inode != 0) {
            if (metadata[i].is_directory) {
                total_directories++;
            } else {
                total_files++;
            }
            
            // Build path index
            std::string path = getFullPath(i);
            if (!path.empty()) {
                path_index.insert(path, i);
            }
            
            if (metadata[i].inode >= next_inode) {
                next_inode = metadata[i].inode + 1;
            }
        }
    }
    
    // Initialize free space bitmap
    uint32_t num_blocks = config.total_size / config.block_size;
    free_blocks.clear();
    free_blocks.resize(num_blocks, true);  // ALL FREE initially
    free_blocks[0] = false;  // Block 0 reserved
    
    std::cout << "Initialized " << num_blocks << " blocks, all marked as FREE" << std::endl;
    
    // Now mark blocks as USED based on actual file data
    int marked_used = 0;
    for (uint32_t i = 1; i <= config.max_files; i++) {
        if (metadata[i].is_valid == 0 && metadata[i].start_block != 0 && metadata[i].inode != 0) {
            // This entry is in use and has blocks
            uint32_t block = metadata[i].start_block;
            int safety = 0;
            
            while (block != 0 && safety < 10000) {
                if (block < free_blocks.size()) {
                    free_blocks[block] = false;  // Mark as USED
                    marked_used++;
                }
                
                // Read next block pointer
                uint8_t block_data[4];
                if (readBlock(block, block_data, 4)) {
                    block = *reinterpret_cast<uint32_t*>(block_data);
                } else {
                    block = 0;  // Stop on error
                }
                
                safety++;
            }
        }
    }
    
    std::cout << "Marked " << marked_used << " blocks as USED" << std::endl;
    
    // Count free blocks
    int free_count = 0;
    for (bool is_free : free_blocks) {
        if (is_free) free_count++;
    }
    
    std::cout << "Free blocks: " << free_count << " / " << num_blocks << std::endl;
    std::cout << "OFS initialized successfully" << std::endl;
    std::cout << "Total files: " << total_files << std::endl;
    std::cout << "Total directories: " << total_directories << std::endl;
    
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// Continued in next part...
// Continuation of ofs_core.cpp - Part 3

void OFSCore::shutdown()
{
    // Write back all changes
    if (omni_file.is_open())
    {
        writeHeader();
        writeUserTable();

        // Write metadata
        for (size_t i = 0; i < metadata.size(); i++)
        {
            writeMetadataEntry(i, metadata[i]);
        }

        omni_file.close();
    }

    // Clean up sessions
    for (Session *session : sessions)
    {
        delete session;
    }
    sessions.clear();

    std::cout << "OFS shutdown complete" << std::endl;
}

int OFSCore::format(const std::string &path, const std::string &config_path)
{
    // Parse configuration
    if (!parseConfig(config_path))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_CONFIG);
    }

    // Create new .omni file
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        return static_cast<int>(OFSErrorCodes::ERROR_IO_ERROR);
    }

    // Initialize header
    std::memset(&header, 0, sizeof(OMNIHeader));
    std::memcpy(header.magic, "OMNIFS01", 8);
    header.format_version = 0x00010000;
    header.total_size = config.total_size;
    header.header_size = config.header_size;
    header.block_size = config.block_size;
    std::memcpy(header.student_id, "BSCS24115", 10);
    header.user_table_offset = config.header_size;
    header.max_users = config.max_users;

    // Write header
    file.write(reinterpret_cast<const char *>(&header), sizeof(OMNIHeader));

    // Calculate offsets
    uint64_t metadata_offset = header.user_table_offset +
                               sizeof(uint32_t) +
                               (config.max_users * sizeof(UserInfo));

    uint64_t free_space_offset = metadata_offset +
                                 (config.max_files * sizeof(MetadataEntry));

    uint32_t num_blocks = config.total_size / config.block_size;

    // Write user table - FIXED: Create admin user properly
    // Write user table - FIXED VERSION
    // CORRECT CODE:
    // Write user count
    uint32_t user_count = 1; // Admin only
    file.write(reinterpret_cast<const char *>(&user_count), sizeof(uint32_t));

    // Create admin user
    UserInfo admin;
    std::memset(&admin, 0, sizeof(UserInfo));
    std::strncpy(admin.username, config.admin_username.c_str(), 31);
    admin.username[31] = '\0';
    std::strncpy(admin.password_hash, hashPassword(config.admin_password).c_str(), 63);
    admin.password_hash[63] = '\0';
    admin.role = UserRole::ADMIN;
    admin.created_time = getCurrentTimestamp();
    admin.last_login = 0;
    admin.is_active = 1; // ACTIVE!

    std::cout << "Created admin user: " << admin.username
              << " with hash: " << std::string(admin.password_hash).substr(0, 16) << std::endl;

    file.write(reinterpret_cast<const char *>(&admin), sizeof(UserInfo));

    // Fill remaining user slots with INACTIVE empty users
    UserInfo empty_user;
    std::memset(&empty_user, 0, sizeof(UserInfo));
    empty_user.is_active = 0; // MARK AS INACTIVE!

    for (uint32_t i = 1; i < config.max_users; i++)
    { // i starts at 1 (admin is slot 0)
        file.write(reinterpret_cast<const char *>(&empty_user), sizeof(UserInfo));

        std::cout << "Created admin user: " << admin.username << " with hash: " << admin.password_hash << std::endl;

        // Write metadata entries (all marked as free)
        MetadataEntry empty_meta;
        std::memset(&empty_meta, 0, sizeof(MetadataEntry));
        empty_meta.is_valid = 1; // Free

        // Entry 0 is reserved
        file.write(reinterpret_cast<const char *>(&empty_meta), sizeof(MetadataEntry));

        // Entry 1 is root directory
        MetadataEntry root;
        std::memset(&root, 0, sizeof(MetadataEntry));
        root.is_valid = 0; // In use
        root.is_directory = 1;
        root.parent_index = 0;
        std::strncpy(root.name, "/", sizeof(root.name) - 1);
        root.name[sizeof(root.name) - 1] = '\0';
        root.start_block = 0;
        root.total_size = 0;
        root.owner_id = 1; // Admin user ID
        root.permissions = 0755;
        root.created_time = getCurrentTimestamp();
        root.modified_time = root.created_time;
        root.inode = 1;
        file.write(reinterpret_cast<const char *>(&root), sizeof(MetadataEntry));

        // Write remaining metadata entries
        for (uint32_t i = 2; i <= config.max_files; i++)
        {
            file.write(reinterpret_cast<const char *>(&empty_meta), sizeof(MetadataEntry));
        }

        // Write free space bitmap (all blocks free except 0)
        for (uint32_t i = 0; i < num_blocks; i++)
        {
            uint8_t is_free = (i == 0) ? 0 : 1;
            file.write(reinterpret_cast<const char *>(&is_free), 1);
        }

        // Pad to total size
        file.seekp(config.total_size - 1);
        file.put(0);

        file.close();

        // Also initialize the in-memory user BST
        users.insert(std::string(admin.username), admin);

        std::cout << "Formatted " << path << " successfully" << std::endl;
        std::cout << "Admin user created: " << admin.username << std::endl;
        return static_cast<int>(OFSErrorCodes::SUCCESS);
    }
}

// ============================================================================
// USER MANAGEMENT
// ============================================================================

int OFSCore::userLogin(void **session, const char *username, const char *password)
{
    UserInfo *user = users.find(std::string(username));

    if (!user || !user->is_active)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    std::string pass_hash = hashPassword(std::string(password));
    if (pass_hash != std::string(user->password_hash))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    // Create new session
    Session *new_session = new Session();
    new_session->session_id = generateSessionId();
    new_session->user_id = next_user_id++; // Simplified UID assignment
    new_session->username = username;
    new_session->role = user->role;
    new_session->login_time = getCurrentTimestamp();
    new_session->last_activity = new_session->login_time;

    user->last_login = new_session->login_time;
    users.insert(std::string(username), *user);

    sessions.push_back(new_session);
    *session = new_session;

    std::cout << "User " << username << " logged in" << std::endl;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::userLogout(void *session)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    // Remove from sessions
    auto it = std::find(sessions.begin(), sessions.end(), sess);
    if (it != sessions.end())
    {
        sessions.erase(it);
    }

    std::cout << "User " << sess->username << " logged out" << std::endl;
    delete sess;

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::userCreate(void *admin_session, const char *username,
                        const char *password, UserRole role)
{
    Session *sess = getSession(admin_session);
    if (!sess || sess->role != UserRole::ADMIN)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    // Check if user already exists
    if (users.find(std::string(username)))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
    }

    // Create new user
    UserInfo new_user(std::string(username), hashPassword(std::string(password)),
                      role, getCurrentTimestamp());
    users.insert(std::string(username), new_user);

    writeUserTable();

    std::cout << "User " << username << " created" << std::endl;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::userDelete(void *admin_session, const char *username)
{
    Session *sess = getSession(admin_session);
    if (!sess || sess->role != UserRole::ADMIN)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    UserInfo *user = users.find(std::string(username));
    if (!user)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    user->is_active = 0;
    users.insert(std::string(username), *user);
    writeUserTable();

    std::cout << "User " << username << " deleted" << std::endl;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::userList(void *admin_session, UserInfo **users_out, int *count)
{
    Session *sess = getSession(admin_session);
    if (!sess || sess->role != UserRole::ADMIN)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    std::vector<UserInfo> user_list = users.getAllValues();
    *count = 0;

    for (const auto &user : user_list)
    {
        if (user.is_active)
            (*count)++;
    }

    *users_out = new UserInfo[*count];
    int idx = 0;
    for (const auto &user : user_list)
    {
        if (user.is_active)
        {
            (*users_out)[idx++] = user;
        }
    }

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::getSessionInfo(void *session, SessionInfo *info)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    std::strncpy(info->session_id, sess->session_id.c_str(), 63);
    info->session_id[63] = '\0';

    UserInfo *user = users.find(sess->username);
    if (user)
    {
        info->user = *user;
    }

    info->login_time = sess->login_time;
    info->last_activity = sess->last_activity;
    info->operations_count = sess->operations_count;

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

int OFSCore::fileCreate(void *session, const char *path, const char *data, size_t size)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    std::string path_str(path);
    std::vector<std::string> components = splitPath(path_str);

    if (components.empty())
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }

    // Get parent directory
    std::string parent_path = "/";
    for (size_t i = 0; i < components.size() - 1; i++)
    {
        parent_path += components[i];
        if (i < components.size() - 2)
            parent_path += "/";
    }

    bool is_dir;
    uint32_t parent_idx = traversePath(parent_path, is_dir);
    if (parent_idx == 0 || !is_dir)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    // Check if file already exists
    uint32_t existing = traversePath(path_str, is_dir);
    if (existing != 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
    }

    // Allocate metadata entry
    uint32_t entry_idx = allocateMetadataEntry();
    if (entry_idx == 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
    }

    // Calculate blocks needed
    size_t content_per_block = config.block_size - sizeof(uint32_t);
    uint32_t blocks_needed = (size + content_per_block - 1) / content_per_block;

    if (blocks_needed == 0)
        blocks_needed = 1;

    std::vector<uint32_t> blocks = allocateBlocks(blocks_needed);
    if (blocks.empty())
    {
        freeMetadataEntry(entry_idx);
        return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
    }

    // Write data to blocks
    size_t data_written = 0;
    for (size_t i = 0; i < blocks.size(); i++)
    {
        uint8_t *block_data = new uint8_t[config.block_size];
        std::memset(block_data, 0, config.block_size);

        // Set next block pointer
        uint32_t next = (i < blocks.size() - 1) ? blocks[i + 1] : 0;
        *reinterpret_cast<uint32_t *>(block_data) = next;

        // Copy data
        size_t to_write = std::min(content_per_block, size - data_written);
        std::memcpy(block_data + sizeof(uint32_t), data + data_written, to_write);

        // Encode data
        encodeData(block_data + sizeof(uint32_t), to_write);

        writeBlock(blocks[i], block_data, config.block_size);
        delete[] block_data;

        data_written += to_write;
    }

    // Create metadata entry
    MetadataEntry &entry = metadata[entry_idx];
    entry.is_valid = 0;
    entry.is_directory = 0;
    entry.parent_index = parent_idx;
    std::strncpy(entry.name, components.back().c_str(), 11);
    entry.name[11] = '\0';
    entry.start_block = blocks[0];
    entry.total_size = size;
    entry.owner_id = sess->user_id;
    entry.permissions = 0644;
    entry.created_time = getCurrentTimestamp();
    entry.modified_time = entry.created_time;
    entry.inode = next_inode++;

    writeMetadataEntry(entry_idx, entry);

    // Add to parent directory
    addToDirectory(parent_idx, entry_idx);

    // Update cache
    path_index.insert(path_str, entry_idx);

    total_files++;
    sess->operations_count++;

    std::cout << "File created: " << path << std::endl;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// Continued...
// Continuation of ofs_core.cpp - Part 4

int OFSCore::fileRead(void *session, const char *path, char **buffer, size_t *size)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t entry_idx = traversePath(std::string(path), is_dir);

    if (entry_idx == 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    if (is_dir)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }

    if (!checkPermission(sess, entry_idx, false))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    MetadataEntry &entry = metadata[entry_idx];
    *size = entry.total_size;
    *buffer = new char[*size + 1];
    (*buffer)[*size] = '\0';

    size_t bytes_read = 0;
    uint32_t block = entry.start_block;
    size_t content_per_block = config.block_size - sizeof(uint32_t);

    while (block != 0 && bytes_read < *size)
    {
        uint8_t *block_data = new uint8_t[config.block_size];
        readBlock(block, block_data, config.block_size);

        uint32_t next_block = *reinterpret_cast<uint32_t *>(block_data);
        uint8_t *content = block_data + sizeof(uint32_t);

        size_t to_read = std::min(content_per_block, *size - bytes_read);

        // Decode data
        decodeData(content, to_read);

        std::memcpy(*buffer + bytes_read, content, to_read);
        bytes_read += to_read;

        delete[] block_data;
        block = next_block;
    }

    sess->operations_count++;
    sess->last_activity = getCurrentTimestamp();

    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::fileEdit(void *session, const char *path, const char *data,
                      size_t size, uint32_t index)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t entry_idx = traversePath(std::string(path), is_dir);

    if (entry_idx == 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    if (is_dir)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }

    if (!checkPermission(sess, entry_idx, true))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    MetadataEntry &entry = metadata[entry_idx];

    // Simple approach: read entire file, modify, write back
    char *current_data;
    size_t current_size;
    int result = fileRead(session, path, &current_data, &current_size);
    if (result != static_cast<int>(OFSErrorCodes::SUCCESS))
    {
        return result;
    }

    // Calculate new size
    size_t new_size = std::max(current_size, static_cast<size_t>(index) + size);
    char *new_data = new char[new_size];

    // Copy existing data
    std::memcpy(new_data, current_data, current_size);

    // Overwrite at index
    if (index + size <= new_size)
    {
        std::memcpy(new_data + index, data, size);
    }

    // Free old blocks
    freeBlockChain(entry.start_block);

    // Write new data
    size_t content_per_block = config.block_size - sizeof(uint32_t);
    uint32_t blocks_needed = (new_size + content_per_block - 1) / content_per_block;

    std::vector<uint32_t> blocks = allocateBlocks(blocks_needed);
    if (blocks.empty())
    {
        delete[] current_data;
        delete[] new_data;
        return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
    }

    size_t data_written = 0;
    for (size_t i = 0; i < blocks.size(); i++)
    {
        uint8_t *block_data = new uint8_t[config.block_size];
        std::memset(block_data, 0, config.block_size);

        uint32_t next = (i < blocks.size() - 1) ? blocks[i + 1] : 0;
        *reinterpret_cast<uint32_t *>(block_data) = next;

        size_t to_write = std::min(content_per_block, new_size - data_written);
        std::memcpy(block_data + sizeof(uint32_t), new_data + data_written, to_write);

        encodeData(block_data + sizeof(uint32_t), to_write);

        writeBlock(blocks[i], block_data, config.block_size);
        delete[] block_data;

        data_written += to_write;
    }

    entry.start_block = blocks[0];
    entry.total_size = new_size;
    entry.modified_time = getCurrentTimestamp();
    writeMetadataEntry(entry_idx, entry);

    delete[] current_data;
    delete[] new_data;

    sess->operations_count++;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::fileDelete(void *session, const char *path)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t entry_idx = traversePath(std::string(path), is_dir);

    if (entry_idx == 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    if (is_dir)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION);
    }

    if (!checkPermission(sess, entry_idx, true))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    MetadataEntry &entry = metadata[entry_idx];

    // Free blocks
    if (entry.start_block != 0)
    {
        freeBlockChain(entry.start_block);
    }

    // Remove from parent directory
    removeFromDirectory(entry.parent_index, entry_idx);

    // Free metadata entry
    freeMetadataEntry(entry_idx);

    // Remove from path index
    path_index.remove(std::string(path));

    total_files--;
    sess->operations_count++;

    std::cout << "File deleted: " << path << std::endl;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::fileTruncate(void *session, const char *path)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t entry_idx = traversePath(std::string(path), is_dir);

    if (entry_idx == 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    if (!checkPermission(sess, entry_idx, true))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    MetadataEntry &entry = metadata[entry_idx];

    // Free all blocks
    if (entry.start_block != 0)
    {
        freeBlockChain(entry.start_block);
    }

    entry.start_block = 0;
    entry.total_size = 0;
    entry.modified_time = getCurrentTimestamp();
    writeMetadataEntry(entry_idx, entry);

    sess->operations_count++;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::fileExists(void *session, const char *path)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t entry_idx = traversePath(std::string(path), is_dir);

    return (entry_idx != 0 && !is_dir) ? static_cast<int>(OFSErrorCodes::SUCCESS) : static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
}

int OFSCore::fileRename(void *session, const char *old_path, const char *new_path)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t entry_idx = traversePath(std::string(old_path), is_dir);

    if (entry_idx == 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    if (!checkPermission(sess, entry_idx, true))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    // Parse new path
    std::vector<std::string> new_components = splitPath(std::string(new_path));
    if (new_components.empty())
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }

    MetadataEntry &entry = metadata[entry_idx];

    // Update name
    std::strncpy(entry.name, new_components.back().c_str(), 11);
    entry.name[11] = '\0';
    entry.modified_time = getCurrentTimestamp();
    writeMetadataEntry(entry_idx, entry);

    // Update path index
    path_index.remove(std::string(old_path));
    path_index.insert(std::string(new_path), entry_idx);

    sess->operations_count++;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// ============================================================================
// DIRECTORY OPERATIONS
// ============================================================================

int OFSCore::dirCreate(void *session, const char *path)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    std::string path_str(path);
    std::vector<std::string> components = splitPath(path_str);

    if (components.empty())
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_PATH);
    }

    // Get parent directory
    std::string parent_path = "/";
    for (size_t i = 0; i < components.size() - 1; i++)
    {
        parent_path += components[i];
        if (i < components.size() - 2)
            parent_path += "/";
    }

    bool is_dir;
    uint32_t parent_idx = traversePath(parent_path, is_dir);
    if (parent_idx == 0 || !is_dir)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    // Check if already exists
    uint32_t existing = traversePath(path_str, is_dir);
    if (existing != 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_FILE_EXISTS);
    }

    // Allocate metadata entry
    uint32_t entry_idx = allocateMetadataEntry();
    if (entry_idx == 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NO_SPACE);
    }

    // Create directory entry
    MetadataEntry &entry = metadata[entry_idx];
    entry.is_valid = 0;
    entry.is_directory = 1;
    entry.parent_index = parent_idx;
    std::strncpy(entry.name, components.back().c_str(), 11);
    entry.name[11] = '\0';
    entry.start_block = 0;
    entry.total_size = 0;
    entry.owner_id = sess->user_id;
    entry.permissions = 0755;
    entry.created_time = getCurrentTimestamp();
    entry.modified_time = entry.created_time;
    entry.inode = next_inode++;

    writeMetadataEntry(entry_idx, entry);

    // Add to parent
    addToDirectory(parent_idx, entry_idx);

    // Update cache
    path_index.insert(path_str, entry_idx);

    total_directories++;
    sess->operations_count++;

    std::cout << "Directory created: " << path << std::endl;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::dirList(void *session, const char *path, FileEntry **entries, int *count)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t dir_idx = traversePath(std::string(path), is_dir);

    if (dir_idx == 0 || !is_dir)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    std::vector<FileEntry> entry_list;
    MetadataEntry &dir = metadata[dir_idx];

    if (dir.start_block != 0)
    {
        uint32_t block = dir.start_block;

        while (block != 0)
        {
            uint8_t *block_data = new uint8_t[config.block_size];
            readBlock(block, block_data, config.block_size);

            uint32_t next_block = *reinterpret_cast<uint32_t *>(block_data);
            uint32_t *children = reinterpret_cast<uint32_t *>(block_data + sizeof(uint32_t));
            size_t max_children = (config.block_size - sizeof(uint32_t)) / sizeof(uint32_t);

            for (size_t i = 0; i < max_children; i++)
            {
                uint32_t child_idx = children[i];
                if (child_idx == 0)
                    break;

                if (child_idx < metadata.size() && metadata[child_idx].is_valid == 0)
                {
                    MetadataEntry &child = metadata[child_idx];

                    FileEntry fe;
                    std::strncpy(fe.name, child.name, 255);
                    fe.name[255] = '\0';
                    fe.type = child.is_directory;
                    fe.size = child.total_size;
                    fe.permissions = child.permissions;
                    fe.created_time = child.created_time;
                    fe.modified_time = child.modified_time;
                    fe.inode = child.inode;

                    entry_list.push_back(fe);
                }
            }

            delete[] block_data;
            block = next_block;
        }
    }

    *count = entry_list.size();
    *entries = new FileEntry[*count];
    for (int i = 0; i < *count; i++)
    {
        (*entries)[i] = entry_list[i];
    }

    sess->operations_count++;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

// Continued...
// Continuation of ofs_core.cpp - Final Part

int OFSCore::dirDelete(void *session, const char *path)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t dir_idx = traversePath(std::string(path), is_dir);

    if (dir_idx == 0 || !is_dir)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    if (dir_idx == 1)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_OPERATION); // Cannot delete root
    }

    if (!checkPermission(sess, dir_idx, true))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    MetadataEntry &dir = metadata[dir_idx];

    // Check if directory is empty
    if (dir.start_block != 0)
    {
        uint8_t *block_data = new uint8_t[config.block_size];
        readBlock(dir.start_block, block_data, config.block_size);

        uint32_t *children = reinterpret_cast<uint32_t *>(block_data + sizeof(uint32_t));
        if (children[0] != 0)
        {
            delete[] block_data;
            return static_cast<int>(OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY);
        }
        delete[] block_data;

        // Free directory blocks
        freeBlockChain(dir.start_block);
    }

    // Remove from parent
    removeFromDirectory(dir.parent_index, dir_idx);

    // Free metadata
    freeMetadataEntry(dir_idx);

    // Remove from cache
    path_index.remove(std::string(path));

    total_directories--;
    sess->operations_count++;

    std::cout << "Directory deleted: " << path << std::endl;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::dirExists(void *session, const char *path)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t dir_idx = traversePath(std::string(path), is_dir);

    return (dir_idx != 0 && is_dir) ? static_cast<int>(OFSErrorCodes::SUCCESS) : static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
}

// ============================================================================
// INFORMATION FUNCTIONS
// ============================================================================

int OFSCore::getMetadata(void *session, const char *path, FileMetadata *meta)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t entry_idx = traversePath(std::string(path), is_dir);

    if (entry_idx == 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    MetadataEntry &entry = metadata[entry_idx];

    std::strncpy(meta->path, path, 511);
    meta->path[511] = '\0';

    std::strncpy(meta->entry.name, entry.name, 255);
    meta->entry.name[255] = '\0';
    meta->entry.type = entry.is_directory;
    meta->entry.size = entry.total_size;
    meta->entry.permissions = entry.permissions;
    meta->entry.created_time = entry.created_time;
    meta->entry.modified_time = entry.modified_time;
    meta->entry.inode = entry.inode;

    // Calculate blocks used
    meta->blocks_used = 0;
    uint32_t block = entry.start_block;
    while (block != 0)
    {
        meta->blocks_used++;
        uint8_t *block_data = new uint8_t[config.block_size];
        readBlock(block, block_data, config.block_size);
        block = *reinterpret_cast<uint32_t *>(block_data);
        delete[] block_data;
    }

    meta->actual_size = meta->blocks_used * config.block_size;

    sess->operations_count++;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::setPermissions(void *session, const char *path, uint32_t permissions)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    bool is_dir;
    uint32_t entry_idx = traversePath(std::string(path), is_dir);

    if (entry_idx == 0)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_NOT_FOUND);
    }

    if (!checkPermission(sess, entry_idx, true))
    {
        return static_cast<int>(OFSErrorCodes::ERROR_PERMISSION_DENIED);
    }

    MetadataEntry &entry = metadata[entry_idx];
    entry.permissions = permissions;
    entry.modified_time = getCurrentTimestamp();
    writeMetadataEntry(entry_idx, entry);

    sess->operations_count++;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

int OFSCore::getStats(void *session, FSStats *stats)
{
    Session *sess = getSession(session);
    if (!sess)
    {
        return static_cast<int>(OFSErrorCodes::ERROR_INVALID_SESSION);
    }

    stats->total_size = config.total_size;

    // Calculate used space
    uint64_t used_blocks = 0;
    for (bool is_free : free_blocks)
    {
        if (!is_free)
            used_blocks++;
    }
    stats->used_space = used_blocks * config.block_size;
    stats->free_space = stats->total_size - stats->used_space;

    stats->total_files = total_files;
    stats->total_directories = total_directories;

    // Count active users
    std::vector<UserInfo> user_list = users.getAllValues();
    stats->total_users = 0;
    for (const auto &user : user_list)
    {
        if (user.is_active)
            stats->total_users++;
    }

    stats->active_sessions = sessions.size();

    // Simple fragmentation calculation
    // Calculate fragmentation properly
    uint32_t free_count = 0;
    uint32_t total_blocks = free_blocks.size();

    for (bool is_free : free_blocks)
    {
        if (is_free)
            free_count++;
    }

    // Fragmentation = used blocks percentage, not free blocks
    uint32_t used_count = total_blocks - free_count;
    stats->fragmentation = (total_blocks > 0) ? (100.0 * used_count / total_blocks) : 0.0;
    sess->operations_count++;
    return static_cast<int>(OFSErrorCodes::SUCCESS);
}

void OFSCore::freeBuffer(void *buffer)
{
    if (buffer)
    {
        delete[] static_cast<char *>(buffer);
    }
}

const char *OFSCore::getErrorMessage(int error_code)
{
    switch (static_cast<OFSErrorCodes>(error_code))
    {
    case OFSErrorCodes::SUCCESS:
        return "Operation completed successfully";
    case OFSErrorCodes::ERROR_NOT_FOUND:
        return "File/directory/user not found";
    case OFSErrorCodes::ERROR_PERMISSION_DENIED:
        return "Permission denied";
    case OFSErrorCodes::ERROR_IO_ERROR:
        return "I/O error occurred";
    case OFSErrorCodes::ERROR_INVALID_PATH:
        return "Invalid path";
    case OFSErrorCodes::ERROR_FILE_EXISTS:
        return "File/directory already exists";
    case OFSErrorCodes::ERROR_NO_SPACE:
        return "No space left on device";
    case OFSErrorCodes::ERROR_INVALID_CONFIG:
        return "Invalid configuration file";
    case OFSErrorCodes::ERROR_NOT_IMPLEMENTED:
        return "Feature not implemented";
    case OFSErrorCodes::ERROR_INVALID_SESSION:
        return "Invalid or expired session";
    case OFSErrorCodes::ERROR_DIRECTORY_NOT_EMPTY:
        return "Directory not empty";
    case OFSErrorCodes::ERROR_INVALID_OPERATION:
        return "Invalid operation";
    default:
        return "Unknown error";
    }
}