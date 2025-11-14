
#include "bitmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ofs_types.h" // This should be the FIRST project include




Bitmap* bitmap_create(size_t bit_count) {
    Bitmap* bm = (Bitmap*)malloc(sizeof(Bitmap));
    if (!bm) return NULL;
    
    bm->bit_count = bit_count;
    bm->byte_count = (bit_count + 7) / 8;
    bm->data = (uint8_t*)calloc(bm->byte_count, 1);
    if (!bm->data) {
        free(bm);
        return NULL;
    }
    return bm;
}

Bitmap* bitmap_load(const uint8_t* data, size_t bit_count) {
    Bitmap* bm = bitmap_create(bit_count);
    if (!bm) return NULL;
    memcpy(bm->data, data, bm->byte_count);
    return bm;
}

void bitmap_save(Bitmap* bm, uint8_t* data, size_t byte_count) {
    if (bm && data && byte_count == bm->byte_count) {
        memcpy(data, bm->data, byte_count);
    }
}

void bitmap_destroy(Bitmap* bm) {
    if (bm) {
        free(bm->data);
        free(bm);
    }
}

void bitmap_set(Bitmap* bm, size_t bit_index) {
    if (bit_index >= bm->bit_count) return;
    bm->data[bit_index / 8] |= (1 << (bit_index % 8));
}

void bitmap_clear(Bitmap* bm, size_t bit_index) {
    if (bit_index >= bm->bit_count) return;
    bm->data[bit_index / 8] &= ~(1 << (bit_index % 8));
}

int bitmap_is_set(Bitmap* bm, size_t bit_index) {
    if (bit_index >= bm->bit_count) return -1;
    return (bm->data[bit_index / 8] & (1 << (bit_index % 8))) != 0;
}

int bitmap_find_first_free(Bitmap* bm) {
    for (size_t i = 0; i < bm->bit_count; i++) {
        if (!bitmap_is_set(bm, i)) {
            return i;
        }
    }
    return -1; // No free blocks
}

// ============================================================================
// bitmap_count_free - Counts the number of unset (free) bits in the bitmap
// ============================================================================
size_t bitmap_count_free(Bitmap* bm) {
    if (!bm) return 0;
    
    size_t free_count = 0;
    // CORRECT: Use bm->bit_count instead of bm->size
    for (size_t i = 0; i < bm->bit_count; i++) {
        // CORRECT: Use bitmap_is_set instead of bitmap_get
        if (!bitmap_is_set(bm, i)) {
            free_count++;
        }
    }
    return free_count;
}