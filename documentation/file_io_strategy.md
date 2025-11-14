# File I/O Strategy for the Omni File System

This document describes the strategies used for reading from, writing to, and managing the single `.omni` container file.

### How You Read From and Write to the `.omni` File

All disk I/O is performed using standard C library functions on the single `FILE* omni_file` pointer held in the `OFSInstance` struct.

*   **Seeking:** The `fseek()` function is used extensively to move the file pointer to the exact byte offset required for an operation. Offsets are calculated based on the locations stored in the `OMNIHeader` (e.g., `header.metadata_offset + (entry_index * sizeof(FileEntry))`).
*   **Reading:** The `fread()` function is used to read raw bytes from the disk into memory (e.g., loading a `FileEntry` struct or a data block).
*   **Writing:** The `fwrite()` function is used to write raw bytes from memory to the disk.

### How You Serialize/Deserialize Standard Structures

Serialization is the process of converting an in-memory struct into a byte stream for storage. For this C implementation, serialization is achieved directly and efficiently:

*   **Serialization (`fwrite`):** Because the on-disk structures (`OMNIHeader`, `FileEntry`) are designed with fixed-size, primitive types, we can write the entire struct to disk in a single operation. `fwrite(&my_struct, sizeof(my_struct), 1, ofs->omni_file)` takes the raw memory representation of the struct and writes it directly to the file.
*   **Deserialization (`fread`):** The reverse is also true. `fread(&my_struct, sizeof(my_struct), 1, ofs->omni_file)` reads the raw bytes from the file and populates the memory of an in-memory struct variable directly.

This approach is simple and fast but assumes that the server reading the file was compiled with the exact same struct definitions.

### Buffering Strategies

*   **Standard Library Buffering:** The primary buffering strategy relies on the default I/O buffering provided by the C standard library. The `fflush(ofs->omni_file)` command is used after critical write operations to ensure that the library's buffer is flushed to the disk, which helps maintain data integrity.
*   **In-Memory Block Buffering:** For operations that modify parts of a data block (like `file_edit`), the entire block is first read from disk into a dynamically allocated in-memory buffer (`malloc`). The changes are made to this buffer in RAM, and then the entire modified buffer is written back to the disk in a single `fwrite` call.

### How You Handle File Growth

In the current implementation, file growth is limited. **Files cannot grow larger than the size of a single data block** (minus a few bytes for size metadata). When a file is created, it is allocated exactly one data block. The `file_edit` operation can increase the file's size up to this limit, but not beyond. A future implementation would require a block-chaining mechanism (like a File Allocation Table or inode pointers) to handle larger files.

### How You Manage Free Space

Free space is managed using a **bitmap**. When a file or directory is created and requires a data block, the `bitmap_find_first_free()` function is called to find the index of the first available block. That bit is then set to `1` using `bitmap_set()`. When a file is deleted or truncated, its corresponding data blocks are freed by clearing the bits back to `0` using `bitmap_clear()`.

### Approach to Data Integrity

Data integrity is handled at a basic level:
*   **Atomic Operations (In-Memory):** The server's single-threaded worker model ensures that file system operations (like a `file_rename`) are atomic from the client's perspective. No other operation can interrupt it.
*   **Flushing:** `fflush()` is used to ensure data is written to disk promptly.
*   **Limitations:** The system **does not use a journal**. If the server crashes in the middle of a write operation (e.g., after updating the bitmap but before writing the `FileEntry`), the on-disk state could become inconsistent.

### What Gets Loaded into Memory vs. Read from Disk

*   **Loaded into Memory at Startup:**
    *   The `OMNIHeader`.
    *   The entire `FileEntry` table, which is then used to build the in-memory `FSTree`.
    *   The entire free block bitmap.
    *   The user table (in a persistent implementation).
*   **Read from Disk Per Operation:**
    *   **File Content:** The actual data inside a file is only read from a data block on disk when a `file_read` or `file_edit` request is processed. This is a crucial optimization to conserve memory.
