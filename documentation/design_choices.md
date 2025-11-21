# Design Choices Document

## 1. Data Structures Chosen

### User Indexing: Custom Hash Table
**Structure:** `UserMap` (Array of Linked Lists for chaining).
**Reasoning:**
The most frequent and security-critical operation is user authentication (`user_login`) and permission checking. A linear search through the user table would be $O(N)$. By implementing a Hash Table using the username as the key, we achieve $O(1)$ average time complexity for lookups. We used chaining (Linked Lists) to handle collisions gracefully without requiring complex resizing logic for the fixed-size user table.

### Directory Tree: Flat Metadata Array
**Structure:** `std::vector<MetadataEntry>` with `parent_index` linking.
**Reasoning:**
Instead of using a pointer-based tree structure which is difficult to serialize to disk, we use a flat array of `MetadataEntry` structs.
- **Representation:** Each entry contains a `uint32_t parent_index`. To list a directory, we iterate the array to find entries where `entry.parent_index` matches the target directory's index.
- **Disk Mapping:** This array maps 1:1 to the Metadata Region in the `.omni` file, allowing for direct block reads/writes.

### Free Space Tracking: In-Memory Bitmap
**Structure:** `std::vector<bool> free_block_map`.
**Reasoning:**
We need to efficiently locate free data blocks for file creation. A bitmap is memory-efficient.
- **Allocation:** Finding $N$ blocks is a linear scan for `true` values.
- **Persistence:** The map is not stored explicitly on disk to save space; instead, it is rebuilt during `fs_init` by scanning the `MetadataEntry` table to see which blocks are currently referenced.

## 2. Omni File Structure
The file system is contained in a single binary file divided into four contiguous regions:
1.  **Header:** `OMNIHeader` struct (Magic bytes, version, offsets).
2.  **User Table:** Fixed region storing `UserInfo` structs.
3.  **Metadata Table:** Fixed region storing `MetadataEntry` structs (inodes).
4.  **Data Blocks:** The remaining space is divided into 4096-byte blocks for raw file content.

## 3. Memory Management
**Strategy:** Hybrid Loading.
- **Metadata:** The Header, User Table, and Metadata Table are loaded entirely into RAM at startup. This ensures directory traversal and permissions are instant.
- **Data:** File content is **not** loaded into memory until `file_read` is requested. This prevents the server from exhausting RAM when storing large files.