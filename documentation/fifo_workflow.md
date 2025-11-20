
# FIFO Queue Implementation in OFS Server

## Overview

The OFS server uses a **FIFO (First-In-First-Out) operation queue** to process client requests in the exact order they arrive. The queue is designed to be **thread-safe**, allowing multiple client threads (producers) and one or more worker threads (consumers) to operate without race conditions.

---

## 1. Request Queue Structure

The request queue uses a **singly linked list**, where each node contains:

```cpp
struct Request {
    int client_sock;        // Client socket descriptor
    std::string request;    // Raw JSON request string
    Request* next;          // Pointer to the next queued request
};
```

The `RequestQueue` class manages `head` and `tail` pointers:

```cpp
class RequestQueue {
private:
    Request* head;
    Request* tail;

public:
    RequestQueue();
    ~RequestQueue();
    void push(int client_sock, const std::string& request);
    Request pop(); // Blocks until a request is available
};
```

### Thread-Safety Components

* `std::mutex rq_mutex` → protects the queue during push/pop
* `std::condition_variable rq_cv` → signals worker threads that a new request is available

---

## 2. Enqueueing Requests (Producer Side)

When a client sends a request, the client-handling thread enqueues it:

```cpp
req_queue.push(client_sock, req);
```

### Push Logic

```cpp
void RequestQueue::push(int client_sock, const std::string& request_str) {
    Request* node = new Request{client_sock, request_str, nullptr};
    std::lock_guard<std::mutex> lock(rq_mutex);

    if (!tail) {
        head = tail = node;
    } else {
        tail->next = node;
        tail = node;
    }

    rq_cv.notify_one(); // Wake worker thread
}
```

### Explanation

* Efficient **O(1)** insertion at the tail
* Linked list avoids resizing overhead
* Mutex ensures only one thread modifies queue at a time
* Condition variable wakes up the worker thread immediately

---

## 3. Dequeueing Requests (Consumer Side)

Worker thread pops requests in FIFO order:

```cpp
Request RequestQueue::pop() {
    std::unique_lock<std::mutex> lock(rq_mutex);

    while (!head)
        rq_cv.wait(lock); // Block until a request arrives

    Request node_copy = *head;
    Request* old = head;
    head = head->next;

    if (!head) tail = nullptr;

    old->next = nullptr;
    delete old;

    return node_copy;
}
```

### Explanation

* `pop()` **blocks** until a request is available
* The worker always receives requests in arrival order
* When queue becomes empty, both `head` and `tail` are set to null

---

## 4. Worker Thread

The server runs a dedicated worker thread:

```cpp
void process_requests() {
    while (true) {
        Request r = req_queue.pop();
        handle_client_request(r.client_sock, r.request);
    }
}
```

### Explanation

* Worker thread continuously waits for new requests
* Processes requests sequentially
* Ensures strict FIFO behavior

---

## 5. Accept Loop & Client Threads

The main server loop accepts new clients, launching a **producer thread** per client:

```cpp
thread([client_sock]() {
    send_msg(client_sock, build_response("WELCOME", "", "message",
        "Welcome to OFS server!", to_string(time(nullptr))));
    
    while (true) {
        string req = recv_msg(client_sock);
        if (req.empty()) break;

        req_queue.push(client_sock, req);
    }

    remove_session(client_sock);
    close(client_sock);
}).detach();
```

### Explanation

* Each client thread listens for messages
* Every received message becomes a queue entry
* When the client disconnects, its session is removed

---

## 6. Session Management Integration

Sessions are stored in a singly linked list:

```cpp
struct ClientSession {
    int client_sock;
    void* session;
    ClientSession* next;
};
```

### Session Functions

* `get_session(client_sock)` → fetch session
* `set_session(client_sock, session)` → assign/update
* `remove_session(client_sock)` → cleanup on disconnect

### Explanation

The worker thread always **validates session** before executing user operations, such as:

* `LOGIN`
* `CREATE`
* `WRITE`
* `DELETE`
* `GET_META`

Unauthorized or expired sessions receive appropriate error codes.

---

## 7. Request Processing Flow

**Complete path of a request:**

1. **Client sends request**
2. **Client thread enqueues request** → `req_queue.push()`
3. **Worker dequeues request** → `req_queue.pop()`
4. **Session validation** → `get_session(client_sock)`
5. **Request parsed & routed** → handled by appropriate manager
6. **Response JSON sent back** → `send_msg()`

Example:
`CREATE` → worker reads file content → VFS writes → metadata updated → response sent.

---

## 8. Summary

* FIFO ensures **fairness and strict order**
* Queue is **thread-safe** (mutex + condition variable)
* Client threads handle **network I/O only**
* Worker thread handles **heavy operations**
* Session system ensures authenticated operations
* Easy to extend for new commands

---

## 9. Visualization (ASCII Diagram)

```
+----------------+        +------------------+        +------------------+
|   Client A     | -----> |                  |        |                  |
|   Client B     | -----> |   FIFO Queue     | -----> |   Worker Thread  |
|   Client C     | -----> |                  |        |                  |
+----------------+        +------------------+        +------------------+
                               ^     |
                               |     v
                        Enqueue req   Dequeue req

Client Request --> Enqueue --> Worker Thread --> Validate Session --> Process --> Response
```

---

