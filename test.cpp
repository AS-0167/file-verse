#include <iostream>
#include <string>
#include <cstring>
#include "HashTable.h"
#include "LinkedList.h"
#include "FSNode.h"
#include "FreeSpaceManager.h"  // Make sure you implemented this

using namespace std;

// Helper to create a FileEntry safely
FileEntry* createFileEntry(const string& name, EntryType type) {
    FileEntry* fe = new FileEntry();
    strncpy(fe->name, name.c_str(), sizeof(fe->name) - 1);
    fe->name[sizeof(fe->name) - 1] = '\0'; // Ensure null-termination
    fe->setType(type);
    return fe;
}

int main() {
    cout << "===== TESTING LINKEDLIST =====\n";
    LinkedList<int*> ll;
    int* a = new int(10);
    int* b = new int(20);
    ll.push_back(a);
    ll.push_back(b);

    auto node = ll.getHead();
    while (node) {
        cout << "LinkedList Node: " << *node->data << endl;
        node = node->next;
    }
    ll.remove(a);
    cout << "After removing a:\n";
    node = ll.getHead();
    while (node) {
        cout << "LinkedList Node: " << *node->data << endl;
        node = node->next;
    }

    cout << "\n===== TESTING HASHTABLE =====\n";
    HashTable<int> ht;
    ht.insert("one", new int(1));
    ht.insert("two", new int(2));

    int* val = ht.get("two");
    if (val) cout << "HashTable get 'two': " << *val << endl;

    ht.remove("one");
    val = ht.get("one");
    cout << "HashTable get 'one' after remove: " << (val ? "found" : "not found") << endl;

    cout << "\n===== TESTING FREESPACEMANAGER =====\n";
    FreeSpaceManager fsm(100);  // 100 blocks
    int start1 = fsm.allocate(10);
    int start2 = fsm.allocate(20);
    cout << "Allocated blocks: 10 at " << start1 << ", 20 at " << start2 << endl;

    fsm.free(start1, 10);
    cout << "Freed 10 blocks starting at " << start1 << endl;

    cout << "\n===== TESTING FSNODE =====\n";
    FileEntry* rootEntry = createFileEntry("root", EntryType::DIRECTORY);
    FSNode* root = new FSNode(rootEntry);

    FileEntry* file1Entry = createFileEntry("file1", EntryType::FILE);
    FSNode* file1 = new FSNode(file1Entry);

    FileEntry* dir1Entry = createFileEntry("dir1", EntryType::DIRECTORY);
    FSNode* dir1 = new FSNode(dir1Entry);

    root->addChild(file1);
    root->addChild(dir1);

    cout << "Children of root:\n";
    auto childNode = root->children->getHead();
    while (childNode) {
        cout << "- " << childNode->data->entry->name << endl;
        childNode = childNode->next;
    }

    cout << "Find 'dir1': " << root->findChild("dir1")->entry->name << endl;
    root->removeChild("file1");
    cout << "After removing file1, children of root:\n";
    childNode = root->children->getHead();
    while (childNode) {
        cout << "- " << childNode->data->entry->name << endl;
        childNode = childNode->next;
    }

    // Cleanup
    delete root;

    return 0;
}
