# FIFO Queue and Server Workflow

This document explains the multi-threaded architecture and the First-In-First-Out (FIFO) request processing workflow of the OFS server.

### How Your Operation Queue Works

The server uses a classic **Producer-Consumer** model to handle requests.

*   **The Queue:** A single, thread-safe FIFO queue (`Queue* request_queue`) acts as the central buffer between the networking part of the server and the core logic.
*   **The Producer:** The main server thread acts as the "producer." Its only job is to listen for new network connections. When a connection is accepted, it reads the incoming JSON request, packages it into a `ClientRequest` struct, and adds it to the back of the queue using `queue_enqueue()`. It then immediately goes back to listening for more connections.
*   **The Consumer:** A single, dedicated "worker thread" acts as the "consumer." Its only job is to pull requests from the front of the queue using `queue_dequeue()`.

This design decouples the acceptance of connections from the processing of requests, allowing the server to be highly responsive to new clients even when busy.

### Thread/Process Management Approach

The server uses a simple and robust multi-threading approach with two primary threads:

1.  **Main Thread (Listener):** This thread is created when the program starts. It initializes the server socket and enters an infinite `while` loop to `accept()` new connections. This thread does not perform any heavy processing.
2.  **Worker Thread:** This thread is created once at server startup. It enters an infinite `while` loop that continuously attempts to dequeue a request from the FIFO queue. Because `queue_dequeue()` is a blocking operation, this thread sleeps efficiently when the queue is empty, consuming no CPU.

Because there is only **one worker thread**, all file system operations are processed **sequentially and in the exact order they were received**. This guarantees data consistency and avoids the complexity and risks of race conditions that would require mutexes or other complex locking mechanisms.

### How Requests are Queued and Processed

1.  A client connects to the server's port.
2.  The **Main Thread**'s `accept()` call returns a new `client_socket`.
3.  The Main Thread reads the entire HTTP/JSON request from the socket into a character buffer.
4.  It dynamically allocates a `ClientRequest` struct (`malloc`). This struct holds the `client_socket` and a copy of the request string (`strdup`).
5.  The Main Thread calls `queue_enqueue()`, which adds the pointer to this new struct to the end of the queue.
6.  The **Worker Thread**, which was waiting on `queue_dequeue()`, wakes up and receives the `ClientRequest` pointer.
7.  The Worker Thread parses the JSON, determines the requested operation, and calls the appropriate core logic function (e.g., `file_create`, `user_login`).
8.  The core logic function completes fully, performing all necessary in-memory and on-disk changes.
9.  The Worker Thread constructs a JSON response.

### How Responses are Sent Back to Clients

1.  After the core logic function returns, the **Worker Thread** builds a JSON response string.
2.  It wraps this JSON string in a minimal HTTP response header.
3.  It sends this complete HTTP response back to the client using the `client_socket` that was stored in the `ClientRequest` struct.
4.  Finally, the Worker Thread `close()`s the client socket and `free()`s the memory used by the `ClientRequest` struct and its copied JSON string.
5.  The Worker Thread immediately loops back to `queue_dequeue()` to process the next request.```

---

The next two documents, `user_guide.md` and `testing_report.md`, are more about how to use and verify your project. I can provide templates for those as well if you'd like to proceed.
