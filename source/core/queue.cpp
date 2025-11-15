#include "include/queue.h"

// ... the rest of the file stays the same ...

void ThreadSafeQueue::push(ClientRequest request) {
    std::lock_guard<std::mutex> lock(mtx);
    q.push(request);
    cv.notify_one(); // Wake up one waiting thread (our worker thread)
}

ClientRequest ThreadSafeQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx);
    // Wait until the condition variable is notified AND the queue is not empty.
    cv.wait(lock, [this]{ return !q.empty(); });
    ClientRequest request = q.front();
    q.pop();
    return request;
}