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

FSNode* FSNode::getChild(const string& name) {
    if (!children) return nullptr;
    LinkedListNode<FSNode*>* curr = children->getHead();
    while (curr) {
        FSNode* child = curr->data;
        if (child->entry && child->entry->name == name)
            return child;
        curr = curr->next;
    }
    return nullptr;
}

FSNode* FSNode::findChild(const string& name) {
    return getChild(name);  // simply call getChild
}

bool FSNode::removeChild(const string& name) {
    FSNode* child = getChild(name);
    if (!child) return false;
    children->remove(child);
    delete child;
    return true;
}

FSNode* FSNode::detachChild(const string& name) {
    FSNode* child = getChild(name);
    if (!child) return nullptr;
    children->remove(child);
    child->parent = nullptr;
    return child;
}

void FSNode::print() const {
    std::cout << (entry->getType() == EntryType::DIRECTORY ? "[DIR] " : "[FILE] ")
              << entry->name << std::endl;
}

vector<FSNode*> FSNode::getChildren() const {
    vector<FSNode*> list;
    if (!children) return list;

    auto node = children->getHead();
    while (node) {
        list.push_back(node->data);
        node = node->next;
    }
    return list;
}

