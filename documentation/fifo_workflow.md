# FIFO Queue Implementation

## 1. Overview
To ensure data consistency without implementing complex file locking (like mutexes per inode), the system uses a **Producer-Consumer** architecture with a thread-safe FIFO (First-In, First-Out) queue. This guarantees that file operations are atomic and sequential.

## 2. Queue Structure
- **Class:** `RequestQueue`
- **Container:** `std::queue<ClientRequest>`
- **Synchronization:**
    - `std::mutex m_mutex`: Protects push and pop operations.
    - `std::condition_variable m_cond`: Used to put the Worker thread to sleep when the queue is empty and wake it up when a request arrives.

## 3. Workflow

### Producer (The Network Layer)
1.  `Server.cpp` listens on port 8080.
2.  Accepts a client connection.
3.  Spawns a detached thread to handle the raw read.
4.  Reads the JSON string from the socket.
5.  Packages `socket_fd` and `json_data` into a `ClientRequest` struct.
6.  Calls `g_request_queue.push()`.

### Consumer (The Worker Layer)
1.  `Main.cpp` runs a continuous loop `worker_loop()`.
2.  Calls `g_request_queue.pop()`. If empty, the thread blocks (waits).
3.  Upon waking with data:
    - Parses the JSON.
    - Calls the relevant `FileSystem` function (e.g., `create_file`).
    - Constructs the JSON response.
    - Sends the response back to the specific `socket_fd`.
    - Closes the socket.
4.  Loops back to process the next item.

This architecture ensures that `file_create` and `file_delete` never run simultaneously, eliminating race conditions on the file system structures.