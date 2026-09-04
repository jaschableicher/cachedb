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
    std::optional<int64_t> set_expiry(
        const std::string& key,
        int64_t expires_in_seconds
    );
    int64_t get_expiry(const std::string& key) const;
private:
    struct Val{
        Value data;
        std::optional<std::int64_t> expiry;
    };
    mutable std::mutex cache_mutex_;
    mutable HashTable<Val> cache_;
};

#endif
