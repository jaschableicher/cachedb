#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <list>
#include <string>
#include <vector>

template <typename V>
class HashTable {
private:
    struct Entry {
        std::string key;
        V data;
    };

    mutable std::vector<std::list<Entry>> buckets_;
    std::size_t index(const std::string& key) const;

public:

    explicit HashTable(std::size_t bucket_count = 16)
        : buckets_(std::max<std::size_t>(bucket_count, 1)) {}

    void insert(const std::string& key, const V& value) {
        auto& bucket = buckets_[index(key)];
        for (auto& entry : bucket) {
            if (entry.key == key) {
                entry.data = value;
                return;
            }
        }
        bucket.push_back(Entry{key, value});
    }

    const V* find(const std::string& key) const {
        const auto& bucket = buckets_[index(key)];
        for (const auto& entry : bucket) {
            if (entry.key == key) return &entry.data;
        }
        return nullptr;
    }

    bool erase(const std::string& key) {
        auto& bucket = buckets_[index(key)];
        for (auto it = bucket.begin(); it != bucket.end(); ++it) {
            if (it->key == key) {
                bucket.erase(it);
                return true;
            }
        }
        return false;
    }

};

template <typename V>
std::size_t HashTable<V>::index(const std::string& key) const {
    return std::hash<std::string>{}(key) % buckets_.size();
}

#endif
