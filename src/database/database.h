#ifndef DATABASE_H
#define DATABASE_H
#include <string>
#include <optional>
#include <mutex>
#include <cstdint>
#include "hashtable.h"
#include "../commands/value.h"
class Database {
public:
    void set(std::string key, Value value);

    std::optional<Value> get(
        const std::string& key
    ) const;

    bool erase(const std::string& key);
    std::optional<std::int64_t> set_expiry(
        const std::string& key,
        std::int64_t expires_in_seconds
    );
private:
    mutable std::mutex cache_mutex_;
    HashTable<Value> cache_;
};

#endif
