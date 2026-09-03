#ifndef DATABASE_H
#define DATABASE_H
#include <string>
#include <optional>
#include <unordered_map>
#include <mutex>
#include "hashtable.h"
#include "../commands/value.h"
class Database {
public:
    void set(std::string key, Value value);

    std::optional<Value> get(
        const std::string& key
    ) const;

    bool erase(const std::string& key);

private:
    mutable std::mutex cache_mutex_;
    HashTable<Value> cache_;
};

#endif