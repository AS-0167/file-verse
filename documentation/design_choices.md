# Design Choices for the Omni File System (OFS)

This document outlines the key data structures and design decisions made during the implementation of the OFS server.

## 1. Data Structures

### User Indexing and Management

*   **Structure Chosen:** A **Hash Table** (`HashTable*`).
*   **Why:** The primary requirement for user management is fast lookup during the `user_login` operation. A hash table provides an average time complexity of **O(1)** for insertions, lookups, and deletions. This is significantly faster than a linear search (O(n)) through an array, especially as the number of users grows.
*   **Implementation Details:** The hash table maps a user's `username` (string key) to a pointer to their `UserInfo` struct (`void* value`). Collision handling is implemented using **chaining with singly-linked lists** (`HashNode* next`).

### Directory Tree Representation

*   **Structure Chosen:** A **Hybrid Approach** using an in-memory tree of `FSNode` structs, where each directory node contains a hash table for its children, and a separate hash table for fast path lookups.
*   **Why:**
    1.  **In-Memory Tree (`FSNode`):** A tree is the natural way to represent a hierarchical file system. Each `FSNode` stores its name, whether it's a directory, and a pointer to its parent, which allows for reconstructing paths.
    2.  **Children Hash Table (`FSNode->children`):** Storing a directory's children in a hash table (mapping child name to `FSNode*`) allows for O(1) lookup of a specific file or subdirectory within a directory. This is crucial for efficiently traversing paths like `/a/b/c`.
    3.  **Global Path Cache (`FSTree->path_cache`):** To avoid traversing the tree from the root for every operation, a global hash table is used to map a full, absolute path string (e.g., `"/docs/report.txt"`) directly to its corresponding `FSNode*`. This provides **O(1) lookup for any file or directory by its full path**, which is a massive performance optimization.

### Free Space Tracking

*   **Structure Chosen:** A **Bitmap** (`Bitmap*`).
*   **Why:** A bitmap is a highly compact and efficient way to track the status of a large number of fixed-size resources (in this case, data blocks). Each bit in the bitmap corresponds to a single data block on disk. A `0` indicates the block is free, and a `1` indicates it is in use. Finding the first available block is a fast, linear scan over the bits. This is much more memory-efficient than a linked list of free blocks.

### File Path to Disk Location Mapping

*   **Structure Chosen:** The **Global Path Cache Hash Table** (`FSTree->path_cache`).
*   **Why:** As described in the directory tree section, this hash table provides a direct, O(1) mapping from a path string to the in-memory `FSNode`. The `FSNode` contains the critical disk location information: `entry_index` (for its metadata location) and `start_block` (for its data location). This is the fastest possible way to get from a path to the physical disk location information.

## 2. `.omni` File Structure

The single `.omni` container file is structured as a contiguous sequence of sections:

1.  **OMNIHeader (First 512 bytes):** A fixed-size header containing "magic numbers" to identify the file type and key metadata like the total size, block size, and the offsets of all other sections.
2.  **FileEntry Table (Metadata Area):** A large, contiguous array of `FileEntry` structs. Each struct represents the on-disk metadata for one file or directory (name, size, owner, timestamps, `start_block`, `parent_index`). Its location is `header.metadata_offset`.
3.  **Free Block Bitmap:** A compact block of bytes representing the bitmap, which tracks the allocation status of the data blocks. Its location is `header.bitmap_offset`.
4.  **Data Blocks:** The remainder of the file, divided into fixed-size (e.g., 4096-byte) blocks where the actual file content is stored. Its location is `header.content_offset`.

## 3. Memory Management Strategies

*   **Load-on-Startup:** For maximum performance, the server loads all metadata into RAM when it starts (`fs_init`). This includes reading the entire `FileEntry` table from disk and using it to construct the in-memory `FSTree` and its `path_cache`. The free block bitmap is also loaded into memory.
*   **Dynamic Allocation:** All in-memory structures (`FSNode`, `HashTable`, `UserInfo`, `SessionInfo`, etc.) are dynamically allocated on the heap using `malloc`/`calloc`.
*   **Manual Cleanup:** Memory is manually freed during operations (`user_delete`, `file_delete`) and completely released when the server shuts down (`fs_shutdown`). The server logic is responsible for freeing any buffers created during operations (e.g., the buffer allocated by `file_read` is freed in `socket_server.c` after the JSON response is created).

## 4. Optimizations

*   **O(1) Path Lookup:** The single most important optimization is the `path_cache` hash table. It eliminates the need for recursive tree traversal for every file operation, making lookups extremely fast regardless of the depth of the directory structure.
*   **In-Memory Metadata:** Keeping all file system metadata in memory avoids slow disk I/O for operations like listing directories (`dir_list`) or checking permissions, which can be satisfied entirely from RAM. Disk access is reserved for reading or writing actual file content.
