# File I/O Strategy

This document describes how the Omni File System (OFS) handles persistent storage, serialization, and memory management.

---

## 1. Overview

OFS uses a **single `.omni` file** to store all persistent data:
- Files and directories (FS tree)
- User accounts
- Free space bitmap
- Filesystem metadata

**Core Strategy:** Full in-memory caching for performance. All data structures are loaded into RAM on initialization (`fs_init`) and written back to disk only on shutdown (`fs_shutdown`), similar to real-world filesystems with write-back caching.

---

## 2. Disk Layout of `.omni` File

| Section | Encrypted? | Description |
|---------|------------|-------------|
| **OMNIHeader** | No | Magic number, version, block size, offsets |
| **FS Tree** | Yes | Recursively serialized directory structure |
| **Free Space Bitmap** | No | Block allocation tracking (0=free, 1=used) |

**Layout:**
```
[OMNIHeader - 512 bytes]
    ↓
[User Table - N × sizeof(UserInfo)]
    ↓
[FS Tree - Variable, encrypted]
    ↓
[Free Space Bitmap - blocks/8 bytes]
```

---

## 3. Serialization & Deserialization

### 3.1 Header and Users (Binary Direct)

**Writing:**
```cpp
ofs.write(reinterpret_cast<const char*>(&header), sizeof(OMNIHeader));
ofs.write(reinterpret_cast<const char*>(&user), sizeof(UserInfo));
```

**Reading:**
```cpp
ifs.read(reinterpret_cast<char*>(&header), sizeof(OMNIHeader));
ifs.read(reinterpret_cast<char*>(&user), sizeof(UserInfo));
```

**Why Direct Binary:**
- Fixed-size structures ensure predictable layout
- No parsing overhead
- Fast read/write operations
- Unencrypted for quick metadata access

### 3.2 FS Tree (Recursive Serialization)

**Structure:**
```cpp
struct FileEntry {
    char name[256];
    bool is_directory;
    uint64_t size;
    time_t created;
    time_t modified;
};
```

**Serialization Steps:**

**For Files:**
1. Write `FileEntry` metadata
2. Write data size (`uint64_t`)
3. Write encrypted file content
4. Write child count = 0

**For Directories:**
1. Write `FileEntry` metadata
2. Write child count (`uint32_t`)
3. Recursively serialize each child

**Example:**
```cpp
void serialize_fs_tree(std::ofstream& ofs, FSNode* node) {
    // Write metadata
    ofs.write(reinterpret_cast<const char*>(&node->entry), sizeof(FileEntry));
    
    if (!node->is_directory) {
        // File: write encrypted data
        uint64_t size = node->data.size();
        ofs.write(reinterpret_cast<const char*>(&size), sizeof(uint64_t));
        
        std::vector<char> encrypted = node->data;
        shift_encrypt(encrypted.data(), encrypted.size(), 1);
        ofs.write(encrypted.data(), encrypted.size());
        
        uint32_t child_count = 0;
        ofs.write(reinterpret_cast<const char*>(&child_count), sizeof(uint32_t));
    } else {
        // Directory: write children
        uint32_t child_count = node->children.size();
        ofs.write(reinterpret_cast<const char*>(&child_count), sizeof(uint32_t));
        
        for (FSNode* child : node->children) {
            serialize_fs_tree(ofs, child);  // Recursive
        }
    }
}
```

**Deserialization:**
```cpp
FSNode* load_fs_tree(std::ifstream& ifs) {
    // Read metadata
    FileEntry entry;
    ifs.read(reinterpret_cast<char*>(&entry), sizeof(FileEntry));
    
    FSNode* node = new FSNode(entry.name, entry.is_directory);
    
    if (!entry.is_directory) {
        // File: read and decrypt data
        uint64_t size;
        ifs.read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
        
        node->data.resize(size);
        ifs.read(node->data.data(), size);
        shift_decrypt(node->data.data(), size, 1);
    }
    
    // Read children
    uint32_t child_count;
    ifs.read(reinterpret_cast<char*>(&child_count), sizeof(uint32_t));
    
    for (uint32_t i = 0; i < child_count; ++i) {
        FSNode* child = load_fs_tree(ifs);  // Recursive
        node->addChild(child);
    }
    
    return node;
}
```

### 3.3 Encryption (Shift Cipher)

**Simple but effective for basic protection:**
```cpp
void shift_encrypt(char* buf, size_t size, int shift) {
    for (size_t i = 0; i < size; ++i) buf[i] += shift;
}

void shift_decrypt(char* buf, size_t size, int shift) {
    for (size_t i = 0; i < size; ++i) buf[i] -= shift;
}
```

**Applied to:**
- FS tree serialization (directory structure)
- File content data

**Not applied to:**
- Header (needs quick validation)
- User table (hashed passwords already secure)
- Bitmap (no sensitive data)

### 3.4 Free Space Bitmap

**Operations:**
```cpp
markUsed(blockIndex);      // Set bit to 1
markFree(blockIndex);      // Set bit to 0
isFree(blockIndex);        // Check bit value
allocate(N);               // Find N contiguous free blocks
free(start, N);            // Free N blocks starting at start
```

**Implementation:**
```cpp
class FreeSpaceManager {
    std::vector<uint8_t> bitmap;  // In-memory
    
    void markUsed(size_t block) {
        bitmap[block / 8] |= (1 << (block % 8));
    }
    
    void markFree(size_t block) {
        bitmap[block / 8] &= ~(1 << (block % 8));
    }
};
```

**Storage:**
- Stored at end of `.omni` file
- Loaded entirely into RAM at initialization
- Written back on shutdown

---

## 4. Memory vs. Disk Strategy

| Component | In RAM? | Disk Read | Disk Write | Notes |
|-----------|---------|-----------|------------|-------|
| **OMNIHeader** | Yes | Once at init | Once at shutdown | Cached for metadata queries |
| **User Table** | Yes | Once at init | Once at shutdown | Hash table for O(1) lookup |
| **FS Tree** | Yes | Once at init | Once at shutdown | Full tree in memory |
| **File Data** | Yes | Once at init | Once at shutdown | Stored in `FSNode::data` |
| **Bitmap** | Yes | Once at init | Once at shutdown | Real-time allocation tracking |

**Key Principle:** Zero disk I/O during runtime. All operations modify in-memory structures.

---

## 5. Buffering Strategy

**No Explicit Buffering Needed:**

Since everything is in RAM, operations are naturally buffered:

```cpp
// File write operation
void file_write(FSNode* node, const std::vector<char>& data) {
    node->data = data;  // Just modify memory
    // No disk I/O!
}

// Only on shutdown:
void fs_shutdown() {
    write_to_omni_file();  // Flush everything to disk
}
```

**Advantages:**
- **Fast operations:** No disk latency
- **Consistency:** All changes are atomic in memory
- **Simple design:** No complex buffer management

**Trade-off:**
- Data loss on crash (no journaling implemented)
- Assumes clean shutdown via `fs_shutdown()`

---

## 6. File Growth Handling

**Dynamic Growth:**
```cpp
class FSNode {
    std::vector<char> data;  // Grows dynamically
    
    void append(const std::vector<char>& new_data) {
        data.insert(data.end(), new_data.begin(), new_data.end());
        // No disk allocation needed - handled at shutdown
    }
};
```

**Block Allocation:**
```cpp
// When serializing at shutdown:
size_t blocks_needed = (data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
std::vector<size_t> blocks = free_space_manager->allocate(blocks_needed);
```

**Truncation:**
```cpp
void file_truncate(FSNode* node, size_t new_size) {
    node->data.resize(new_size);  // Shrink in memory
    // Bitmap updated at shutdown
}
```

---

## 7. Free Space Management

**Runtime Behavior:**
- Bitmap tracks which blocks are used/free
- File creation allocates blocks conceptually
- File deletion marks blocks free
- All changes in RAM only

**Allocation Algorithm:**
```cpp
std::vector<size_t> allocate(size_t count) {
    std::vector<size_t> blocks;
    size_t consecutive = 0;
    
    for (size_t i = 0; i < total_blocks; ++i) {
        if (isFree(i)) {
            consecutive++;
            if (consecutive == count) {
                // Found enough blocks
                for (size_t j = i - count + 1; j <= i; ++j) {
                    markUsed(j);
                    blocks.push_back(j);
                }
                return blocks;
            }
        } else {
            consecutive = 0;
        }
    }
    
    return {};  // Not enough space
}
```

**Fragmentation:**
- First-fit algorithm may fragment over time
- Defragmentation not implemented (simplification)
- Acceptable for project scope

---

## 8. Data Integrity

**Strategies:**

1. **Atomic Memory Operations:**
   - All FS modifications happen in RAM
   - Either all changes succeed or none (no partial writes)

2. **Sequential Write Order:**
   ```
   1. OMNIHeader (validates file format)
   2. User Table
   3. FS Tree (encrypted)
   4. Bitmap (reflects current state)
   ```

3. **Encryption Layer:**
   - Prevents casual file tampering
   - Detectable if file manually edited

4. **Fixed-Size Structures:**
   - `OMNIHeader` and `UserInfo` have predictable offsets
   - Reduces corruption risk from pointer errors

**Limitations (Acknowledged Trade-offs):**
- No journaling/logging
- No crash recovery
- No checksums/CRC validation
- Requires clean shutdown

**Future Improvements:**
- Write-ahead log (WAL) for crash recovery
- Periodic snapshots
- Checksum validation

---

## 9. Complete Workflow Examples

### Initialization (`fs_init`)

```cpp
1. Open .omni file for reading
2. Read OMNIHeader → validate magic number
3. Read User Table → populate HashTable
4. Read FS Tree → decrypt and reconstruct in RAM
5. Read Bitmap → initialize FreeSpaceManager
6. Close file (no longer needed until shutdown)
```

### Shutdown (`fs_shutdown`)

```cpp
1. Open .omni file for writing
2. Write OMNIHeader
3. Write User Table
4. Serialize and encrypt FS Tree → write
5. Write Bitmap
6. Close file
7. Free all in-memory resources
```

### File Creation (`file_create`)

```cpp
1. Create FSNode in memory
2. Allocate blocks in bitmap (in RAM)
3. Add to parent's children list
4. Write data to FSNode::data vector
5. No disk I/O
```

### File Read (`file_read`)

```cpp
1. Traverse tree to find FSNode
2. Return FSNode::data directly (already in RAM)
3. No disk I/O
```

### File Delete (`file_delete`)

```cpp
1. Find FSNode in tree
2. Mark blocks as free in bitmap
3. Remove from parent's children
4. Delete FSNode (free RAM)
5. No disk I/O
```

---

## 10. Performance Characteristics

| Operation | Time Complexity | Disk I/O |
|-----------|----------------|----------|
| File read | O(depth) tree traversal | None |
| File write | O(1) + O(data size) | None |
| File create | O(depth + blocks) | None |
| File delete | O(depth + blocks) | None |
| Directory list | O(children) | None |
| Init | O(total files) | Full read |
| Shutdown | O(total files) | Full write |

**Bottleneck:** Shutdown time increases with filesystem size (all data written at once).

---

## 11. Summary

**Key Design Decisions:**

**Single `.omni` file** for portability  
**Full in-memory caching** for speed  
**Recursive serialization** for hierarchical structure  
**Simple shift cipher** for basic protection  
**Bitmap** for efficient space management  
**Direct binary I/O** for fixed structures  
**No buffering needed** (everything in RAM)  


This design balances **simplicity, performance, and educational value** for a filesystem implementation project.

---

**End of Document**