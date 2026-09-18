#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <list>
#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
template <typename V>
class HashTable {
private:
    struct Entry {
        std::string key;
        V data;
    };
     struct Bucket {
        std::list<Entry> list;
        mutable std::shared_mutex mtx; // Local lock
    };


    std::vector<Bucket> buckets_;
    std::atomic<std::size_t> entry_count_ = 0;
    mutable std::shared_mutex resize_mutex_;


    std::size_t index(const std::string& key,std::size_t bucket_count) const;

    void rehash(uint64_t new_size){
        
        std::unique_lock<std::shared_mutex> global_lock(resize_mutex_);
        //Recheck incase other thread rehashed
        if (entry_count_.load() <= static_cast<std::size_t>(buckets_.size() * 4)) {
            return; 
        }
        std::vector<Bucket> new_buckets(new_size);
        
        for (auto& old_bucket : buckets_) {
            while(!old_bucket.list.empty()){
                const size_t new_idx = index(old_bucket.list.front().key, new_size);
                new_buckets[new_idx].list.splice(
                    new_buckets[new_idx].list.begin(), // destination position
                    old_bucket.list,                   // source list
                    old_bucket.list.begin()            // iterator to element
                );
            }
        }
        buckets_ = std::move(new_buckets);
    }
public:
     const std::vector<std::list<Entry>>& buckets() const noexcept {
        return buckets_;
    }
    explicit HashTable(std::size_t bucket_count = 8192)
        : buckets_(std::max<std::size_t>(bucket_count, 1)) {}

    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;


    auto begin() const noexcept { return buckets_.begin(); }
    auto end() const noexcept   { return buckets_.end(); }

    void insert(const std::string& key, const V& value) {
        std::shared_lock<std::shared_mutex> global_lock(resize_mutex_);
        size_t idx = index(key, bucket_count());
        auto& bucket = buckets_[idx];
        {
            std::unique_lock<std::shared_mutex> lock(bucket.mtx);
            for (auto& entry : bucket.list) {
                if (entry.key == key) {
                    entry.data = value;
                    return;
                }
            }
            buckets_[idx].list.push_back(Entry{key, value});
            entry_count_.fetch_add(1, std::memory_order_relaxed);
        }
        //Max 4 values per list allowed
        if (entry_count_.load() > static_cast<std::size_t>(buckets_.size() *4)) {
            // Release our shared lock before rehashing to avoid self-deadlock
            global_lock.unlock(); 
            rehash(buckets_.size() * 8);
        }
    }

    const V* find(const std::string& key) const {
        const auto& bucket = buckets_[index(key,bucket_count())];
        std::shared_lock<std::shared_mutex> lock(bucket.mtx);

        for (const auto& entry : bucket.list) {
            if (entry.key == key) return &entry.data;
        }
        return nullptr;
    }

    bool erase(const std::string& key) {
        std::shared_lock<std::shared_mutex> global_lock(resize_mutex_);
        auto& bucket = buckets_[index(key, bucket_count())];
        std::unique_lock<std::shared_mutex> lock(bucket.mtx);
        for (auto it = bucket.list.begin(); it != bucket.list.end(); ++it) {
            if (it->key == key) {
                bucket.list.erase(it);
                entry_count_.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
        }
        return false;
    }
    size_t bucket_count() const noexcept{
        return buckets_.size();
    }
    std::size_t size() const noexcept {
        return entry_count_.load(std::memory_order_relaxed);
    }
};

template <typename V>
std::size_t HashTable<V>::index(const std::string& key,std::size_t bucket_count) const {
    return std::hash<std::string>{}(key) % bucket_count;
}

#endif
