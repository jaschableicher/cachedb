#include "hashtable.h"

#include "../commands/value.h"

#include <algorithm>
#include <chrono>
#include <functional>

namespace {
std::int64_t current_epoch_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
}  // namespace

template <typename V>
HashTable<V>::HashTable(std::size_t bucket_count)
    : buckets_(std::max<std::size_t>(bucket_count, 1)) {}

template <typename V>
std::size_t HashTable<V>::index(const std::string& key) const {
    return std::hash<std::string>{}(key) % buckets_.size();
}

template <typename V>
void HashTable<V>::insert(const std::string& key, const V& value) {
    auto& bucket = buckets_[index(key)];

    for (auto& entry : bucket) {
        if (entry.key == key) {
            entry.data.value = value;
            return;
        }
    }

    bucket.push_back(Entry{key, Val{value, std::nullopt}});
}

template <typename V>
const V* HashTable<V>::find(const std::string& key) const {
    auto& bucket = buckets_[index(key)];
    for (auto it = bucket.begin(); it != bucket.end(); ++it) {
        if (it->key == key) {
            if (it->data.expiry.has_value() &&
                *it->data.expiry <= current_epoch_seconds()) {
                bucket.erase(it);
                return nullptr;
            }
            return &it->data.value;
        }
    }
    return nullptr;
}

template <typename V>
bool HashTable<V>::erase(const std::string& key) {
    auto& bucket = buckets_[index(key)];
    for (auto it = bucket.begin(); it != bucket.end(); ++it) {
        if (it->key == key) {
            bucket.erase(it);
            return true;
        }
    }
    return false;
}

template <typename V>
bool HashTable<V>::set_expiry(const std::string& key, std::int64_t expiry) {
    auto& bucket = buckets_[index(key)];
    for(auto it = bucket.begin();it!=bucket.end();++it){
        if(it->key==key){
            it->data.expiry=expiry;
            return true;
        }
    }
    return false;
}

template <typename V>
void HashTable<V>::erase_expired() {
    const auto now = current_epoch_seconds();
    for (auto& bucket : buckets_) {
        bucket.remove_if([now](const Entry& entry) {
            return entry.data.expiry.has_value() && *entry.data.expiry <= now;
        });
    }
}

template class HashTable<Value>;
