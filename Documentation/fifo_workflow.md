 FIFO Queue Implementation

This document explains the threading and queueing model used by the server.

 How your operation queue works

The server uses a thread-safe, First-In, First-Out (FIFO) queue to manage all incoming client requests. This queue is implemented as a `ThreadSafeQueue` class that wraps a standard `std::queue`.

   Mutual Exclusion: A `std::mutex` is used to ensure that only one thread can access the queue at any given time, preventing corruption.
   Efficient Waiting: A `std::condition_variable` is used to manage the worker thread. Instead of constantly checking if the queue has items (busy-waiting), the worker thread sleeps until the condition variable is notified. When a new request is pushed to the queue, the pushing thread notifies the condition variable, which wakes up the worker thread to process the new item.

 Thread/process management approach

The server uses a multi-threaded, single-worker model:

1.  Main Thread: The main thread initializes the file system and starts the server's listening socket. Its only job is to wait for new client connections.
2.  Client Handler Threads: For each new client that connects, the main thread immediately spawns a new, short-lived `handle_client` thread. This thread is "detached," meaning the main thread does not wait for it to finish.
3.  Worker Thread: A single, long-running `worker_thread_function` is created when the server starts. This is the only thread that ever performs file system operations.

This approach allows the server to accept many connections quickly while ensuring that all modifications to the file system happen sequentially and safely.

 How requests are queued and processed

1.  Queuing (Producers): When a client connects, its dedicated `handle_client` thread reads the JSON request from the socket. It packages the client's socket descriptor and the request data into a `ClientRequest` struct. It then locks the queue, pushes this struct onto the back, and notifies the condition variable. The handler thread's job is now done.

2.  Processing (Consumer): The single `worker_thread` is constantly waiting on the queue. When it is woken up, it locks the queue, pops the next `ClientRequest` from the front, and unlocks the queue. It then parses the JSON, performs all the logic for the requested operation (e.g., creating a file, checking a password), and generates a JSON response.

 How responses are sent back to clients

After the worker thread has processed a request and created a JSON response, it converts the response object into a string (`.dump()`). It then uses the `client_socket` that was stored in the `ClientRequest` struct to send the response string directly back to the correct client. Finally, it closes the client's socket connection.