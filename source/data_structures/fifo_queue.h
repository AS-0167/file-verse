#ifndef FIFO_QUEUE_H
#define FIFO_QUEUE_H

#include <queue>
#include <stdexcept>

template <typename T>
class FIFOQueue {
private:
    std::queue<T> q;

public:
    void enqueue(const T& item) {
        q.push(item);
    }

    T dequeue() {
        if (q.empty()) {
            throw std::out_of_range("Queue is empty");
        }
        T item = q.front();
        q.pop();
        return item;
    }

    bool isEmpty() const {
        return q.empty();
    }

    std::size_t size() const {
        return q.size();
    }

    T& front() {
        if (q.empty()) {
            throw std::out_of_range("Queue is empty");
        }
        return q.front();
    }

    const T& front() const {
        if (q.empty()) {
            throw std::out_of_range("Queue is empty");
        }
        return q.front();
    }
};

#endif
