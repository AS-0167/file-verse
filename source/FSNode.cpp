#include "FSNode.h"

FSNode::FSNode(FileEntry* e, FSNode* p)
    : entry(e), parent(p) {
    if (entry->getType() == EntryType::DIRECTORY)
        children = new LinkedList<FSNode*>();
    else
        children = nullptr;
}

FSNode::~FSNode() {
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

void FSNode::addChild(FSNode* child) {
    if (entry->getType() == EntryType::DIRECTORY && children) {
        children->push_back(child);
        child->parent = this;
    }
}

bool FSNode::removeChild(const string& name) {
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

FSNode* FSNode::detachChild(const string& name) {
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

FSNode* FSNode::findChild(const string& name) {
    if (!children) return nullptr;
    auto node = children->getHead();
    while (node) {
        if (node->data->entry->name == name)
            return node->data;
        node = node->next;
    }
    return nullptr;
}

void FSNode::print() const {
    cout << (entry->getType() == EntryType::DIRECTORY ? "[DIR] " : "[FILE] ")
         << entry->name << endl;
}