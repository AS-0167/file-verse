#include <string>
#include <iostream>
#include "odf_types.hpp"

template <typename T>
struct LinkedListNode {
    T* data;
    LinkedListNode* next;

    LinkedListNode(T* d) : data(d), next(nullptr) {}
};

template <typename T>
class LinkedList {
private:
    LinkedListNode<T>* head;

public:
    LinkedList() : head(nullptr)
    {}

    ~LinkedList() {
        LinkedListNode<T>* current = head;
        while (current) {
            LinkedListNode<T>* temp = current;
            current = current->next;
            delete temp->data; 
            delete temp;
        }
    }

    void push_back(T* data) {
        LinkedListNode<T>* node = new LinkedListNode<T>(data);
        if (!head) {
            head = node;
            return;
        }
        LinkedListNode<T>* curr = head;
        while (curr->next) curr = curr->next;
        curr->next = node;
    }

    void remove(T* data) {
        LinkedListNode<T>* curr = head;
        LinkedListNode<T>* prev = nullptr;
        while (curr) {
            if (curr->data == data) {
                if (prev) prev->next = curr->next;
                else head = curr->next;
                delete curr->data;
                delete curr;
                return;
            }
            prev = curr;
            curr = curr->next;
        }
    }

    LinkedListNode<T>* getHead() const
     {
         return head;
     }
};
