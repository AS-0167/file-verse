#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include <string>

struct Request {
    int client_sock;
    std::string request; // the raw request string
    Request* next;
};

class RequestQueue {
private:
    Request* head;
    Request* tail;
    // non-copyable
    RequestQueue(const RequestQueue&) = delete;
    RequestQueue& operator=(const RequestQueue&) = delete;

public:
    RequestQueue();
    ~RequestQueue();

    // Enqueue a request (thread-safe)
    void push(int client_sock, const std::string& request);

    // Pop next request (blocks until available). Caller owns returned Request (by value).
    Request pop();
};

#endif // REQUEST_QUEUE_H
