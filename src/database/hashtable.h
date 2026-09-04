#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <vector>

template <typename V>
class HashTable {
private:
    struct Val {
        V value;
        std::optional<std::int64_t> expiry;
    };
    struct Entry {
        std::string key;
        Val data;
    };

    mutable std::vector<std::list<Entry>> buckets_;
    std::size_t index(const std::string& key) const;

public:

    explicit HashTable(std::size_t bucket_count = 16);

    void insert(const std::string& key, const V& value);
    const V* find(const std::string& key) const;
    bool erase(const std::string& key);
    bool set_expiry(const std::string& key, std::int64_t expiry);
    void erase_expired();
};

#endif
