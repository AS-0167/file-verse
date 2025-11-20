#ifndef QUEUE_H
#define QUEUE_H

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

// This struct represents a single request from a client
struct ClientRequest {
    int client_socket;
    std::string request_data;
};

// This is our thread-safe queue
class ThreadSafeQueue {
public:
    void push(ClientRequest request);
    ClientRequest pop();

private:
    std::queue<ClientRequest> q;
    std::mutex mtx;
    std::condition_variable cv;
};

#endif // QUEUE_H