#ifndef QUEUE_H
#define QUEUE_H

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

// Represents a single request received from a client.
struct ClientRequest {
    int client_socket;
    std::string request_data;
};

// A thread-safe queue to hold client requests.
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