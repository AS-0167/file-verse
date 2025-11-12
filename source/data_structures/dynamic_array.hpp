#pragma once
#include <cstddef>
#include <new>
#include <utility>

template<typename T>
class DynamicArray {
    T* data;
    int n;  // size
    int c;  // capacity

    void destroy_elements() {
        for (int i = 0; i < n; ++i) data[i].~T();
        n = 0;
    }

    void grow(int newCap) {
        if (newCap <= c) return;
        T* nd = static_cast<T*>(::operator new[](newCap * sizeof(T)));

for (int i = 0; i < n; ++i) {
            new (nd + i) T(std::move(data[i]));
            data[i].~T();
        }
        ::operator delete[](data);
        data = nd;
        c = newCap;
    }

public:
    DynamicArray() : data(nullptr), n(0), c(0) {}
    ~DynamicArray() {
        destroy_elements();
        ::operator delete[](data);
    }


    DynamicArray(const DynamicArray&) = delete;
    DynamicArray& operator=(const DynamicArray&) = delete;

    DynamicArray(DynamicArray&& o) noexcept : data(o.data), n(o.n), c(o.c) {
        o.data = nullptr; o.n = 0; o.c = 0;
    }
    DynamicArray& operator=(DynamicArray&& o) noexcept {
        if (this != &o) {
            destroy_elements();
            ::operator delete[](data);
            data = o.data; n = o.n; c = o.c;
            o.data = nullptr; o.n = 0; o.c = 0;
        }
        return *this;
    }


    int getSize() const { return n; }
    bool empty() const { return n == 0; }

    T& operator[](int i) { return data[i]; }
    const T& operator[](int i) const { return data[i]; }

    void push_back(const T& v) {
        if (n == c) grow(c ? c * 2 : 8);
        new (data + n) T(v);
        ++n;
    }
    void push_back(T&& v) {
        if (n == c) grow(c ? c * 2 : 8);
        new (data + n) T(std::move(v));
        ++n;
    }

    void clear() { destroy_elements(); }

    void swap(DynamicArray& o) noexcept {
        std::swap(data, o.data);
        std::swap(n, o.n);
        std::swap(c, o.c);
    }
};
