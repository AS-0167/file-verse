#include "../../include/RequestQueue.h"

void RequestQueue::push(ClientRequest request) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(request);
    m_cond.notify_one();
}

ClientRequest RequestQueue::pop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cond.wait(lock, [this]{ return !m_queue.empty(); });
    ClientRequest request = m_queue.front();
    m_queue.pop();
    return request;
}