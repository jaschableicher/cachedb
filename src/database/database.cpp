#include "database.h"

#include <chrono>

void Database::set(std::string key, Value value) {
    std::scoped_lock lock(cache_mutex_);
    cache_.insert(key, value);
}

std::optional<Value> Database::get(const std::string& key) const{
    std::scoped_lock lock(cache_mutex_);
    const Value* value = cache_.find(key);
    if(value == nullptr) return std::nullopt;
    return *value;
}

bool Database::erase(const std::string& key){
    std::scoped_lock lock(cache_mutex_);
    
    return cache_.erase(key);
}

std::optional<std::int64_t> Database::set_expiry(
    const std::string& key,
    std::int64_t expires_in_seconds
) {
    std::scoped_lock lock(cache_mutex_);
    const auto epoch_now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch());
    const std::int64_t expiry = epoch_now.count() + expires_in_seconds;
    if(cache_.set_expiry(key,expiry)){
        return expires_in_seconds;
    }
    return std::nullopt;
}
