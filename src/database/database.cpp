#include "database.h"

void Database::set(std::string key, std::string value) {
    std::scoped_lock lock(cache_mutex_);
    cache_[key]=value;
}

std::optional<std::string> Database::get(const std::string& key) const{
    std::scoped_lock lock(cache_mutex_);
    auto it = cache_.find(key);
    if(it==cache_.end()) return std::nullopt;
    return cache_.at(key);      

}

bool Database::erase(const std::string& key){
    std::scoped_lock lock(cache_mutex_);
    bool key_exists = cache_.contains(key);
    if(!key_exists) return false;
    cache_.erase(key);
    return true;
}