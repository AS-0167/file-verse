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
    
    FSNode(FileEntry* e, FSNode* p = nullptr);
    ~FSNode();
    
    FSNode(const FSNode&) = delete;
    FSNode& operator=(const FSNode&) = delete;
    
    void addChild(FSNode* child);
    bool removeChild(const string& name);
    FSNode* detachChild(const string& name);
    FSNode* findChild(const string& name);
    void print() const;
};

#endif // FSNODE_H
