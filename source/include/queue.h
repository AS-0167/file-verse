// queue.h
#pragma once
#include <string>
#include <mutex>
#include <condition_variable>

struct Request {
    int client_sock;
    std::string raw_message;
};

// Node for linked list
struct Node {
    Request data;
    Node* next;
    Node(const Request& r) : data(r), next(nullptr) {}
};

class RequestQueue {
private:
    Node* head;
    Node* tail;
    std::mutex mtx;
    std::condition_variable cv;

public:
    RequestQueue() : head(nullptr), tail(nullptr) {}

    void push(const Request& req) {
        Node* node = new Node(req);
        std::unique_lock<std::mutex> lock(mtx);
        if (!tail) head = tail = node;
        else { tail->next = node; tail = node; }
        cv.notify_one();
    }

    Request pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return head != nullptr; });

        Node* node = head;
        head = head->next;
        if (!head) tail = nullptr;

        Request req = node->data;
        delete node;
        return req;
    }

    bool empty() {
        std::unique_lock<std::mutex> lock(mtx);
        return head == nullptr;
    }
};
