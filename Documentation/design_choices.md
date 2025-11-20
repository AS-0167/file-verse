Design Choices Document

This document explains the core design decisions made for the Omni File System (OFS).

Data Structures Chosen and WHY

 What structure did you use for user indexing and why?

I chose a Hash Table for user indexing. The primary reason is performance.

   Why: The `user_login` operation requires finding a user by their username as quickly as possible. A hash table provides an average time complexity of O(1) for lookups. This is significantly faster than searching through a list or vector, which would be O(n).
   Implementation: The hash table maps a username (string) to a pointer to the `UserInfo` struct within the main `user_table` vector. This avoids duplicating user data in memory. Collisions are handled using chaining (a linked list of users at the same hash index).

 How do you represent the directory tree?

The directory tree is represented implicitly using a flat vector of `MetadataEntry` structs.

   How: Each file or directory in the `metadata_entries` vector has a `parent_index` field. This integer is the index of its parent directory within the same vector. The root directory (`/`) is always at index `0` and its `parent_index` points to itself.
   Why: This is a simple and memory-efficient way to represent a tree without using complex pointer-based node structures. Traversing the tree involves scanning this vector to find children of a given parent index.

 What structure tracks free space?

A bitmap, implemented as a `std::vector<bool>`, is used to track free data blocks.

   How: The `free_block_map` vector has an entry for every data block in the file system. `true` means the block is free, and `false` means it is in use.
   Why: This structure is very simple and fast. Finding a free block is a quick linear scan over the vector. It has a very low memory footprint, as each 4096-byte block is represented by a single bit (plus vector overhead).

 How do you map file paths to disk locations?

File paths are mapped to disk locations via the `find_entry_by_path` helper function.

   How: This function takes a full path like `/docs/report.txt` and splits it into segments (`"docs"`, `"report.txt"`). It then traverses the `metadata_entries` vector level by level, starting from the root (index 0). At each level, it searches for an entry with the correct name and the correct `parent_index`.
   Once the final entry is found, its `start_index` gives the data block number. The final on-disk byte offset is calculated using this block number and the file system's header information.

 How you structured the .omni file

The `.omni` file is a single, contiguous binary file with a simple, fixed layout:

1.  OMNIHeader (512 bytes): A fixed-size header at the very beginning containing metadata about the entire file system (e.g., total size, block size, offsets).
2.  User Table: A fixed-size array of `UserInfo` structs, located immediately after the header.
3.  Metadata Table: A fixed-size array of `MetadataEntry` structs, located after the user table. This acts as the "inode table" for the file system.
4.  Data Blocks: The remainder of the file is divided into fixed-size data blocks (e.g., 4096 bytes each) where actual file content is stored.

 Memory Management Strategies

The primary strategy is to load all metadata into memory on startup.

   During `fs_init`, the `OMNIHeader`, the entire `User Table`, and the entire `Metadata Table` are read from the `.omni` file and stored in the global `FileSystemInstance` struct.
   A `free_block_map` is also constructed in memory by scanning the metadata table to see which blocks are in use.
   Trade-off: This uses more RAM but makes all file system operations (like listing directories or finding files) extremely fast, as they do not require repeated disk access. The only time the disk is read during an operation is for `file_read`.
   All writes (`file_create`, `user_delete`, etc.) are written directly to the `.omni` file and flushed to ensure persistence.

 Any Optimizations Made

The main optimization is the hash table for user lookups, which dramatically speeds up the `user_login` process from O(n) to O(1) on average. The decision to load all metadata into memory is also a major performance optimization for all path-based operations at the cost of higher memory usage.