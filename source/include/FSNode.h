#ifndef FSNODE_H
#define FSNODE_H
#include <string>
#include <iostream>
#include "odf_types.hpp"
#include "LinkedList.h"
using namespace std;

class FSNode {
public:
    FileEntry* entry;             
    LinkedList<FSNode*>* children; 
    FSNode* parent;               

    FSNode(FileEntry* e, FSNode* p = nullptr)
        : entry(e), parent(p) {
        if (entry->getType() == EntryType::DIRECTORY)
            children = new LinkedList<FSNode*>();
        else
            children = nullptr;
    }

    ~FSNode() {
    
        if (children) {
            auto node = children->getHead();
            while (node) {
                delete node->data;  
                node = node->next;
            }
            delete children;
        }
        delete entry; 
    }


    FSNode(const FSNode&) = delete;
    FSNode& operator=(const FSNode&) = delete;

    void addChild(FSNode* child) {
        if (entry->getType() == EntryType::DIRECTORY && children) {
            children->push_back(child);
            child->parent = this;
        }
    }

    
    bool removeChild(const string& name) {
        if (!children) return false;
        
        auto node = children->getHead();
        while (node) {
            if (node->data->entry->name == name) {
                FSNode* child = node->data;
                children->remove(child);  
                delete child;             
                return true;
            }
            node = node->next;
        }
        return false;
    }

    FSNode* detachChild(const string& name) {
        if (!children) return nullptr;
        
        auto node = children->getHead();
        while (node) {
            if (node->data->entry->name == name) {
                FSNode* child = node->data;
                children->remove(child);  
                child->parent = nullptr;  
                return child;            
            }
            node = node->next;
        }
        return nullptr;
    }

    FSNode* findChild(const string& name) {
        if (!children) return nullptr;
        
        auto node = children->getHead();
        while (node) {
            if (node->data->entry->name == name)
                return node->data;
            node = node->next;
        }
        return nullptr;
    }

    void print() const {
        cout << (entry->getType() == EntryType::DIRECTORY ? "[DIR] " : "[FILE] ")
             << entry->name << endl;
    }
};

#endif // FSNODE_H

