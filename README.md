# OFS - The Omni File System

OFS is a multi-user, networked file system implemented entirely in C. It is built from scratch, featuring its own in-memory data structures, a single-file binary storage format (`.omni`), and a JSON-based API for all operations.

The server uses a multi-threaded, producer-consumer architecture to handle multiple simultaneous client connections sequentially, ensuring data consistency without the need for complex locking.

## Features

*   **Full CRUD Operations:** Create, Read, Edit, Delete, and Rename files.
*   **Directory Management:** Create, List, and Delete directories.
*   **Multi-User System:** Secure session management with `login` and `logout`.
*   **Admin-Gated Permissions:** Role-based access control for administrative tasks like creating new users.
*   **Networked JSON API:** All operations are exposed via a simple, `curl`-compatible API.
*   **High Performance:** All file system metadata is indexed in memory using hash tables for O(1) lookups, ensuring fast performance regardless of file system size.

## Quick Start Guide

This guide explains how to get the OFS server running.

### 1. Prerequisites

*   A C compiler (e.g., `gcc`)
*   The `make` build tool
*   The `json-c` development library
    *   On Debian/Ubuntu: `sudo apt-get install libjson-c-dev`
    *   On Fedora/CentOS: `sudo dnf install json-c-devel`

### 2. Compile

Navigate to the project's root directory and run `make`:

```bash
make
This will compile the server and place the executable in the compiled/ directory.
3. Format the Filesystem (First Time Only)
Before the first run, create a fresh .omni container file:
code
Bash
make format
Warning: This will erase any existing sample.omni file.
4. Run the Server
Start the server using the provided make command:
code
Bash
make run
The server will now be running and listening on http://localhost:8080.
Example Usage with curl
Here is a quick example of how to interact with the server.
1. Login and get a session ID
code
Bash
curl -X POST -H "Content-Type: application/json" -d '{"operation": "user_login", "parameters": {"username": "admin", "password": "admin123"}}' http://localhost:8080
Response: {"status":"success","data":{"session_id":"admin_17631XXXXXX"}}
2. Create a directory (using your new session ID)
code
Bash
curl -X POST -H "Content-Type: application/json" -d '{"operation": "dir_create", "session_id": "admin_17631XXXXXX", "parameters": {"path": "/documents"}}' http://localhost:8080
Response: {"status":"success"}
3. Create and read a file
code
Bash
# Create the file
curl -X POST -H "Content-Type: application/json" -d '{"operation": "file_create", "session_id": "admin_17631XXXXXX", "parameters": {"path": "/documents/hello.txt", "data": "Hello from OFS!"}}' http://localhost:8080

# Read the file
curl -X POST -H "Content-Type: application/json" -d '{"operation": "file_read", "session_id": "admin_17631XXXXXX", "parameters": {"path": "/documents/hello.txt"}}' http://localhost:8080
Response: {"status":"success","data":{"content":"Hello from OFS!"}}
For more detailed documentation on design choices, file I/O strategy, and testing, please see the files in the documentation/ directory.
