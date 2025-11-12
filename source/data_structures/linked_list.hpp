#pragma once
#include <cstddef>
#include <utility>

template<typename T>
class LinkedList {
    struct Node {
        T val;
        Node* next;
        Node(const T& x) : val(x), next(nullptr) {}
        Node(T&& x) : val(std::move(x)), next(nullptr) {}
    };

    Node* head;
    Node* tail;
    int count;

public:

    LinkedList() : head(nullptr), tail(nullptr), count(0) {}
    ~LinkedList() { clear(); }


    LinkedList(LinkedList&& other) noexcept
        : head(other.head), tail(other.tail), count(other.count) {
        other.head = other.tail = nullptr;
        other.count = 0;
    }
    LinkedList& operator=(LinkedList&& other) noexcept {
        if (this != &other) {
            clear();
            head = other.head; tail = other.tail; count = other.count;
            other.head = other.tail = nullptr;
            other.count = 0;
        }
        return *this;
    }
    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;


    void push_back(const T& x) {
        Node* n = new Node(x);
        if (tail) tail->next = n; else head = n;
        tail = n; ++count;
    }
    void push_back(T&& x) {
        Node* n = new Node(std::move(x));
        if (tail) tail->next = n; else head = n;
        tail = n; ++count;
    }

    int size() const { return count; }
    bool empty() const { return count == 0; }

    void clear() {
        Node* cur = head;
        while (cur) {
            Node* nx = cur->next;
            delete cur;
            cur = nx;
        }
        head = tail = nullptr;
        count = 0;
    }

    struct iterator {
        Node* cur;
        T& operator*() const { return cur->val; }
        iterator& operator++() { cur = cur->next; return *this; }
        bool operator!=(const iterator& o) const { return cur != o.cur; }
    };
    iterator begin() { return { head }; }
    iterator end()   { return { nullptr }; }

    void swap(LinkedList& o) noexcept {
        std::swap(head, o.head);
        std::swap(tail, o.tail);
        std::swap(count, o.count);
    }
};
