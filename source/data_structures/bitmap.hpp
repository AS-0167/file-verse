#pragma once
#include <cstdint>
#include <cstring>
#include <utility>

class Bitmap {
    unsigned char* data;
    int bits;

public:

    Bitmap() : data(nullptr), bits(0) {}
    ~Bitmap() { delete[] data; }


    Bitmap(Bitmap&& other) noexcept : data(other.data), bits(other.bits) {
        other.data = nullptr; other.bits = 0;
    }
    Bitmap& operator=(Bitmap&& other) noexcept {
        if (this != &other) {
            delete[] data;
            data = other.data; bits = other.bits;
            other.data = nullptr; other.bits = 0;
        }
        return *this;
    }
    Bitmap(const Bitmap&) = delete;
    Bitmap& operator=(const Bitmap&) = delete;


    void reset(int n) {
        delete[] data; data = nullptr;
        bits = n;
        int bytes = (n + 7) / 8;
        data = (bytes > 0) ? new unsigned char[bytes] : nullptr;
        if (data) std::memset(data, 0, bytes);
    }

    int size() const { return bits; }

    void set(int i)  { data[i >> 3] |=  (1 << (i & 7)); }
    void clear(int i){ data[i >> 3] &= ~(1 << (i & 7)); }
    bool test(int i) const { return (data[i >> 3] >> (i & 7)) & 1; }

    int find_free_run(int need) const {
        if (need <= 0) return -1;
        int run = 0, start = 0;
        for (int i = 0; i < bits; i++) {
            if (!test(i)) { if (run == 0) start = i; if (++run == need) return start; }
            else run = 0;
        }
        return -1;
    }
};
