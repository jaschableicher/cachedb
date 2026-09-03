#include "database.h"

void Database::set(std::string key, Value value) {
    std::scoped_lock lock(cache_mutex_);
    cache_.insert(key, value);
}

std::optional<Value> Database::get(const std::string& key) const{
    std::scoped_lock lock(cache_mutex_);
    Value* value = cache_.find(key);
    if(value == nullptr) return std::nullopt;
    return *value;    
}

bool Database::erase(const std::string& key){
    std::scoped_lock lock(cache_mutex_);
    
    return cache_.erase(key);
}