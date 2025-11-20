#include "../../include/queue.h"

void ThreadSafeQueue::push(ClientRequest request) {
    std::lock_guard<std::mutex> lock(mtx); // Lock the mutex
    q.push(request);                      // Add the item
    cv.notify_one();                      // Notify one waiting thread
}

ClientRequest ThreadSafeQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx); // Lock the mutex
    cv.wait(lock, [this]{ return !q.empty(); }); // Wait until the queue is not empty
    ClientRequest request = q.front();
    q.pop();
    return request;
}