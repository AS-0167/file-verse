#ifndef BITMAP_H
#define BITMAP_H

#include <stdint.h>
#include <stddef.h>

typedef struct Bitmap {
    uint8_t* data;
    size_t bit_count;
    size_t byte_count;
} Bitmap;

Bitmap* bitmap_create(size_t bit_count);
Bitmap* bitmap_load(const uint8_t* data, size_t bit_count);
void bitmap_save(Bitmap* bm, uint8_t* data, size_t byte_count);
void bitmap_destroy(Bitmap* bm);
void bitmap_set(Bitmap* bm, size_t bit_index);
void bitmap_clear(Bitmap* bm, size_t bit_index);
int bitmap_is_set(Bitmap* bm, size_t bit_index);
int bitmap_find_first_free(Bitmap* bm);

#endif // BITMAP_H