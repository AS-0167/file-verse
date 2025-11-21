#include "RequestQueue.h"
#include <mutex>
#include <condition_variable>

static std::mutex rq_mutex;
static std::condition_variable rq_cv;

RequestQueue::RequestQueue() : head(nullptr), tail(nullptr) {}

RequestQueue::~RequestQueue() {
    std::lock_guard<std::mutex> lock(rq_mutex);
    Request* cur = head;
    while (cur) {
        Request* nxt = cur->next;
        delete cur;
        cur = nxt;
    }
    head = tail = nullptr;
}

void RequestQueue::push(int client_sock, const std::string& request_str) {
    Request* node = new Request{client_sock, request_str, nullptr};

    std::lock_guard<std::mutex> lock(rq_mutex);
    if (!tail) {
        head = tail = node;
    } else {
        tail->next = node;
        tail = node;
    }
    rq_cv.notify_one();
}

Request RequestQueue::pop() {
    std::unique_lock<std::mutex> lock(rq_mutex);
    while (!head) {
        rq_cv.wait(lock);
    }
    // detach node
    Request node_copy = *head; // copy contents
    Request* old = head;
    head = head->next;
    if (!head) tail = nullptr;
    // old->next points into copied memory; set to nullptr to avoid double-delete confusion
    old->next = nullptr;
    delete old;
    return node_copy;
}

