#ifndef FREESPACEMANAGER_H
#define FREESPACEMANAGER_H
#include <iostream>
#include <vector>
#include <cstdint>
using namespace std;

class FreeSpaceManager {
private:
    uint64_t totalBlocks;
    vector<uint8_t> bitmap; 
    
public:
    FreeSpaceManager(uint64_t total_blocks);
    void markUsed(uint64_t blockIndex);
    void markFree(uint64_t blockIndex);
    bool isFree(uint64_t blockIndex) const;
    int64_t findFreeBlocks(uint64_t N);
    void printBitmap() const;
    int64_t allocate(uint64_t N);
    void free(uint64_t start, uint64_t N);
   void setBitmap(const std::vector<uint8_t>& b);
   const vector<uint8_t>& getBitmap() const;
    uint64_t getLargestFreeBlock();

};

#endif

