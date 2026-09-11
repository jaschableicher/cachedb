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

    std::vector<std::list<Entry>> buckets_;
    std::size_t entry_count_ = 0;
    std::size_t index(const std::string& key) const;


    void rehash(uint64_t new_size){
        std::vector<std::list<Entry>> new_buckets(new_size);
        for (auto& old_bucket : buckets_) {
            while(!old_bucket.empty()){
                const size_t new_idx = std::hash<std::string>{}(old_bucket.front().key)%new_buckets.size();
                new_buckets[new_idx].splice(
                    new_buckets[new_idx].begin(), // destination position
                    old_bucket,                   // source list
                    old_bucket.begin()            // iterator to element
                );
            }
        }
        buckets_ = std::move(new_buckets);
    }
public:
     const std::vector<std::list<Entry>>& buckets() const noexcept {
        return buckets_;
    }
    explicit HashTable(std::size_t bucket_count = 256)
        : buckets_(std::max<std::size_t>(bucket_count, 1)) {}



    auto begin() const noexcept { return buckets_.begin(); }
    auto end() const noexcept   { return buckets_.end(); }

    void insert(const std::string& key, const V& value) {
        //TODO: implement custom vector growing/rehashing of values
        //Every time it is done it should be num_items * 2 in size So it stays O(1) at most of the time
        //-->Uper limit of vector size? As it cannot grow indefinetly

        auto& bucket = buckets_[index(key)];
        for (auto& entry : bucket) {
            if (entry.key == key) {
                entry.data = value;
                return;
            }
        }
        if (static_cast<double>(entry_count_) + 1.0 >
        static_cast<double>(buckets_.size()) * 0.9) {
        rehash(buckets_.size() * 2);
    }

        buckets_[index(key)].push_back(Entry{key, value});
        ++entry_count_;
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
                --entry_count_;
                return true;
            }
        }
        return false;
    }
    size_t bucket_count() const noexcept{
        return buckets_.size();
    }
    std::size_t size() const noexcept {
        return entry_count_;
    }
};

template <typename V>
std::size_t HashTable<V>::index(const std::string& key) const {
    return std::hash<std::string>{}(key) % buckets_.size();
}

#endif
