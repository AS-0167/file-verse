#include "FreeSpaceManager.h"

FreeSpaceManager::FreeSpaceManager(uint64_t total_blocks) : totalBlocks(total_blocks) {
    uint64_t bytes_needed = (totalBlocks + 7) / 8;
    bitmap.resize(bytes_needed, 0); 
}

void FreeSpaceManager::markUsed(uint64_t blockIndex) {
    uint64_t byteIndex = blockIndex / 8;
    uint8_t bitIndex = blockIndex % 8;
    bitmap[byteIndex] |= (1 << bitIndex);
}

void FreeSpaceManager::markFree(uint64_t blockIndex) {
    uint64_t byteIndex = blockIndex / 8; 
    uint8_t bitIndex = blockIndex % 8;
    bitmap[byteIndex] &= ~(1 << bitIndex);
}

bool FreeSpaceManager::isFree(uint64_t blockIndex) const {
    uint64_t byteIndex = blockIndex / 8;
    uint8_t bitIndex = blockIndex % 8;
    return !(bitmap[byteIndex] & (1 << bitIndex));
}

int64_t FreeSpaceManager::findFreeBlocks(uint64_t N) {
    uint64_t consecutive = 0;
    uint64_t start = 0;
    for (uint64_t i = 0; i < totalBlocks; ++i) {
        if (isFree(i)) {
            if (consecutive == 0) start = i;
            ++consecutive;
            if (consecutive == N) return start;
        } else {
            consecutive = 0;
        }
    }
    return -1; 
}

void FreeSpaceManager::printBitmap() const {
    for (uint64_t i = 0; i < totalBlocks; ++i) {
        std::cout << (isFree(i) ? '0' : '1');
        if ((i+1) % 64 == 0) 
            cout << "\n";
    }
    cout << std::endl;
}

int64_t FreeSpaceManager::allocate(uint64_t N) {
    int64_t start = findFreeBlocks(N);
    if (start == -1) return -1; 
    for (uint64_t i = 0; i < N; ++i) {
        markUsed(start + i);
    }
    return start;
}

void FreeSpaceManager::free(uint64_t start, uint64_t N) {
    for (uint64_t i = 0; i < N; ++i) {
        markFree(start + i);
    }
}

void FreeSpaceManager:: setBitmap(const std::vector<uint8_t>& b) 
{
     bitmap = b; 
}
const vector<uint8_t>& FreeSpaceManager:: getBitmap() const 
{ 
    
    return bitmap; 
}
uint64_t FreeSpaceManager::getLargestFreeBlock() {
    uint64_t largest = 0;
    uint64_t current = 0;

    for (uint64_t i = 0; i < totalBlocks; ++i) {
        if (isFree(i)) {
            current++;
            if (current > largest)
                largest = current;
        } else {
            current = 0;
        }
    }

    return largest;
}
