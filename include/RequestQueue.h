#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>

struct ClientRequest {
    int client_socket;
    std::string request_data;
};

class RequestQueue {
public:
    void push(ClientRequest request);
    ClientRequest pop();

private:
    std::queue<ClientRequest> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond;
};

#endif // REQUEST_QUEUE_H