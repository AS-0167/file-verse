 File I/O Strategy

This document describes how the Omni File System interacts with the `.omni` container file on disk.

 How you read from and write to the .omni file

All file I/O is performed using standard C++ file streams: `std::ifstream` for reading and `std::fstream` for reading and writing.

   Reading: To read a specific piece of data (like a struct or a data block), the program opens the file, uses `seekg()` to move the read pointer to the exact byte offset, and then calls `read()`.
   Writing: To write data, the program opens the file, uses `seekp()` to move the write pointer to the correct location, and calls `write()`.

 How you serialize/deserialize standard structures

Serialization is done by writing the raw binary representation of our C++ structs (`OMNIHeader`, `UserInfo`, `MetadataEntry`) directly to the file.

   We use `reinterpret_cast<const char>()` to treat the struct as a simple array of bytes.
   `file.write()` is then used to write these bytes to disk.
   Deserialization is the reverse process: `file.read()` reads bytes from the file directly into the memory location of a struct instance.

This strategy is simple and fast but requires that all structs have a fixed size and a consistent memory layout.

 Buffering strategies

The implementation does not use a custom buffering layer. It relies on the default buffering provided by the C++ `fstream` library. However, to ensure data is written to disk immediately and prevent inconsistencies, `file.flush()` is called after every `write()` operation before the file is closed.

 How you handle file growth

The file system does not support file growth. The `.omni` file is created with a fixed, maximum size during the `fs_format` process. All data must fit within this pre-allocated space.

 How you manage free space

Free space is managed for two different resources:

1.  Metadata Entries: The `metadata_entries` vector is fixed. An entry is considered "free" if its `validity_flag` is set to `1`. `find_free_metadata_entry()` performs a linear scan to find the first available slot.
2.  Data Blocks: A bitmap (`std::vector<bool>`) named `free_block_map` is kept in memory. `true` indicates a free block, `false` indicates an occupied one. `find_free_block()` performs a linear scan to find the first available block.

When a file is deleted, its metadata entry's flag is reset, and the corresponding bit in the `free_block_map` is set back to `true`.

 Approach to data integrity

Data integrity is primarily enforced by two key design choices:

1.  Serialized Operations: The FIFO queue ensures that only one file system operation (read, write, delete, etc.) can run at a time. This completely prevents race conditions where two threads might try to modify the same resource simultaneously.
2.  Immediate Flushing: Calling `file.flush()` after every write operation requests that the operating system commit the changes from its cache to the physical disk, reducing the risk of data loss or corruption if the server crashes.

 What gets loaded into memory vs. read from disk per operation

   Loaded into Memory on Startup (`fs_init`):
       The `OMNIHeader`.
       The entire `User Table`.
       The entire `Metadata Table`.
       The `free_block_map` is constructed and also kept in memory.

   Read from Disk Per Operation:
       Only the content of files is read from disk on demand. When a `file_read` command is executed, the program seeks to the appropriate data block in the `.omni` file and reads only the required bytes.