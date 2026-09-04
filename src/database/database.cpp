#include "database.h"

#include <chrono>

void Database::set(std::string key, Value value) {
    std::scoped_lock lock(cache_mutex_);
    cache_.insert(key, {value});
}

std::optional<Value> Database::get(const std::string& key) const{
    std::scoped_lock lock(cache_mutex_);
    const Val* value = cache_.find(key);
    if(value == nullptr) return std::nullopt;
    return value->data;
}

bool Database::erase(const std::string& key){
    std::scoped_lock lock(cache_mutex_);
    
    return cache_.erase(key);
}

std::optional<std::int64_t> Database::set_expiry(const std::string& key,int64_t expires_in_seconds) {
    std::scoped_lock lock(cache_mutex_);
    const Val* cache_val = cache_.find(key);
    if (cache_val == nullptr) return std::nullopt;

    const auto epoch_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
    const std::int64_t expiry = epoch_now.count() + expires_in_seconds;
    Val updated = *cache_val;
    updated.expiry = expiry;
    cache_.insert(key, updated);
    return expires_in_seconds;
}

int64_t Database::get_expiry(const std::string& key) const{
    std::scoped_lock lock(cache_mutex_);
    const Val* cache_val = cache_.find(key);
    if(cache_val == nullptr) return -1;
    
    return cache_val->expiry.has_value()? cache_val->expiry.value():-1;
}
