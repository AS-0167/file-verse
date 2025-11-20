# OFS – Design Choices

This document explains the key design decisions made during the implementation of the **OFS (Omni File System)**, including data structures, memory management, file storage, and optimizations.

---

## 1. User Indexing

**Structure Used:** `HashTable<UserInfo>`

**Reasoning:**

* User operations such as login require **O(1) lookup by username**.
* `HashTable<UserInfo>` maps `username → UserInfo` for fast access.
* Collisions are handled via internal chaining.
* Active sessions are tracked using `std::vector<SessionInfo*>` for efficient iteration over logged-in users.

**Example Usage:**

```cpp
users->get(username);  // Retrieve user info during login
```

---

## 2. Directory Tree Representation

**Structure Used:** `FSNode` with `LinkedList<FSNode*> children`

**Reasoning:**

* The directory structure is hierarchical; each `FSNode` represents a file or directory.
* `children` stored as a linked list for **efficient insertion and deletion**.
* `parent` pointer allows traversal upwards in the tree.
* Path resolution and searching are recursive, using methods like `find_node_by_path()`.

**Key Functions:**

```cpp
addChild(FSNode* child);
removeChild(const std::string& name);
FSNode* find_node_by_path(const std::string& path);
```

---

## 3. Free Space Tracking

**Structure Used:** `FreeSpaceManager` using a **bitmap** (`std::vector<uint8_t>`)

**Reasoning:**

* Each bit represents a disk block: free or used.
* Efficient allocation/deallocation of blocks.
* Supports queries like `findFreeBlocks()` and `getLargestFreeBlock()` for contiguous space allocation.

**Optimizations:**

* Bitmaps are memory-efficient compared to full descriptors for each block.
* Cached largest free block speeds up allocation.

---

## 4. File Path → Disk Mapping

**Structure Used:**

* `FSNode` contains `FileEntry* entry` metadata and `std::vector<char> data`.
* Each node corresponds to a directory entry; `FileEntry` maps the logical file path to physical blocks in the `.omni` file.

**Reasoning:**

* Separates logical path resolution (tree traversal) from physical storage.
* Simplifies operations like `file_create`, `file_read`, and `file_delete`.

---

## 5. `.omni` File Structure

**Components:**

1. **Header (`OMNIHeader`)** – stores file system version, block size, total size.
2. **Data Blocks** – raw file content.
3. **Indexing** – maps `FSNode`/`FileEntry` to corresponding blocks.
4. **User Table** – `HashTable<UserInfo>` for authentication.



Byte 0:       Header - 512 bytes

              Magic: "OMNI"

              Version: 1.0

              Block Size: 4096

              Offsets to other sections

Byte 512:   

            Bitmap - 32 KB for 262,144 blocks

            Each bit represents one 4KB block

After Bitmap: 

              User Table - Variable size

              Serialized hash table of users

After Users:  
            
              FS Tree - Variable size

              Serialized directory structure

After Tree:   
              Data Blocks - 4KB × N blocks

              Block 0
              Block 1
              

**Reasoning:**

* Centralized storage makes the FS portable.
* Header + indexing allows fast FS initialization and lookups.

---

## 6. Memory Management Strategies

* `FSNode` dynamically allocates memory for `children` and `data`.
* Destructors recursively free child nodes to prevent memory leaks.
* `FreeSpaceManager` manages allocation/deallocation of disk blocks.
* Copy constructors for `FSNode` are deleted to avoid accidental deep copies:

```cpp
FSNode(const FSNode&) = delete;
FSNode& operator=(const FSNode&) = delete;
```

**Optimizations:**

* File data stored in `std::vector<char>` for dynamic resizing.
* Linked lists in directories allow cheap insertions and deletions.

---
## 7. File Memory Management

* RAII: `std::vector<char>` for file data auto-manages memory
* Recursive Cleanup:  `~FSNode()` deletes all children recursively
* No Copying: Copy constructors deleted to prevent expensive deep copies
* Raw Pointers: Parent owns children; clear ownership avoids shared_ptr circular reference issues
```cpp
~FSNode() {
    for (FSNode* child : children) delete child;  // Recursive
    if (entry) delete entry;
    // vector<char> data auto-deletes
}
```


## 8. Additional Optimizations

* Sessions stored in vectors for fast iteration.
* Hash table ensures constant-time user access, critical for authentication-heavy operations.
* Linked lists for children optimize operations in directories with frequent changes.

---

**End of Document**
