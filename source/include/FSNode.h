#ifndef FSNODE_H
#define FSNODE_H

#include <string>
#include <vector>
#include <iostream>
#include "LinkedList.h"
#include "odf_types.hpp"

class FSNode {
public:
    FileEntry* entry;
    std::vector<char> data;
    LinkedList<FSNode*>* children;
    FSNode* parent;

    FSNode(FileEntry* e, FSNode* p = nullptr);
    ~FSNode();

    FSNode(const FSNode&) = delete;
    FSNode& operator=(const FSNode&) = delete;

    void addChild(FSNode* child);
    bool removeChild(const std::string& name);
    FSNode* detachChild(const std::string& name);

    FSNode* getChild(const std::string& name);      // main search
    FSNode* findChild(const std::string& name);     // alias for getChild
    vector<FSNode*> getChildren() const;


    void print() const;
};

#endif
