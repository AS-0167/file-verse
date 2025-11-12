#pragma once
#include <cstddef>
#include <utility>
#include <string>
#include "dynamic_array.hpp"
#include "linked_list.hpp"

template<typename K> struct SimpleHash { std::size_t operator()(const K& k) const { return std::hash<K>{}(k); } };
template<> struct SimpleHash<std::string> {
    std::size_t operator()(const std::string& s) const { std::size_t h=5381; for(unsigned char c: s) h=((h<<5)+h)+c; return h; }
};

template<typename K, typename V, typename H=SimpleHash<K>>
class HashMap {
    DynamicArray<LinkedList<std::pair<K, V>>> buckets;
    int count;

    int index(const K& key) const { return H{}(key) & (buckets.getSize() - 1); }

    void rehash(int n) {
        DynamicArray<LinkedList<std::pair<K, V>>> newBuckets;
        for (int i = 0; i < n; i++)
            newBuckets.push_back(LinkedList<std::pair<K, V>>{});

        for (int i = 0; i < buckets.getSize(); i++) {
            for (auto it = buckets[i].begin(); it != buckets[i].end(); ++it) {
                int j = H{}((*it).first) & (n - 1);
                newBuckets[j].push_back(*it);
            }
        }
        buckets = std::move(newBuckets);
    }

    void ensure() {
        if (buckets.getSize() == 0)
            for (int i = 0; i < 8; i++) buckets.push_back(LinkedList<std::pair<K, V>>{});
        if (count * 4 >= buckets.getSize() * 3)
            rehash(buckets.getSize() * 2);
    }

public:
    HashMap() : count(0) { rehash(8); }

    int size() const { return count; }
    bool empty() const { return count == 0; }

    void put(const K& key, const V& val) {
        ensure();
        auto& b = buckets[index(key)];
        for (auto it = b.begin(); it != b.end(); ++it)
            if ((*it).first == key) { (*it).second = val; return; }
        b.push_back({ key, val });
        count++;
    }

    V* get(const K& key) {
        auto& b = buckets[index(key)];
        for (auto it = b.begin(); it != b.end(); ++it)
            if ((*it).first == key) return &(*it).second;
        return nullptr;
    }
};

