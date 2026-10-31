#ifndef DATABASE_H
#define DATABASE_H
#include <string>
#include <optional>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <thread>
#include <queue>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <variant>
#include <vector>
#include <list>
#include <unordered_map>
#include "hashtable.h"
#include "../commands/value.h"
#include "expiration.h"
enum SnapshotReturn{
    ErrorCorruptedFile,
    ErrorInvalidFormat,
    ErrorMalformedHeader,
    ErrorNewerVersion,
    ErrorReadingBucketCount,
    Success
};
class Database {
public:
    Database();
    ~Database();
    void set(std::string key, const Value& value);

    std::optional<Value> get(
        const std::string& key
    );

    bool erase(const std::string& key);
    std::optional<int64_t> set_expiry(
        const std::string& key,
        int64_t expires_in_seconds
    );
    int64_t get_expiry(const std::string& key);


    //Memory Dumps
    bool create_memory_snapshot(std::string& filename);
    SnapshotReturn read_memory_snapshot(std::string& filename);
private:
    struct Val{
        Value data;
        std::optional<std::int64_t> expiry;
        uint8_t version=0;
    };
    mutable std::mutex cache_mutex_;
    mutable HashTable<Val> cache_;

    ExpirationHeap expiration_heap_;
    std::unordered_map<std::string, ExpirationEntry> expiration_entries_;
    std::atomic_bool running_;
    std::thread expiration_thread;
    std::mutex expiration_mutex_;
    std::condition_variable expiration_condition_;
    

    bool delete_if_expired(const Val* cache_value, const std::string& key);
    void active_expiration_check();
};

#endif
