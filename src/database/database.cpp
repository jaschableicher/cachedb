#include "database.h"

#include <chrono>

void Database::set(std::string key, Value value) {
    std::scoped_lock lock(cache_mutex_);
    cache_.insert(key, {value}); // If expiry was set before it is now automatically unset!
}

std::optional<Value> Database::get(const std::string& key){
    std::scoped_lock lock(cache_mutex_);
    const Val* cache_val = cache_.find(key);
    if(cache_val == nullptr) return std::nullopt;
    if(delete_if_expired(cache_val, key)) return std::nullopt;
    return cache_val->data;
}

bool Database::erase(const std::string& key){
    std::scoped_lock lock(cache_mutex_);
    //Should here also check for expiry as technically if expired it should return false as nothing was supposed to be there in the first place?
    return cache_.erase(key);
}

std::optional<std::int64_t> Database::set_expiry(const std::string& key,int64_t expires_in_seconds) {
    
    std::scoped_lock lock(cache_mutex_);
    const Val* cache_val = cache_.find(key);
    if (cache_val == nullptr) return std::nullopt;
    if(delete_if_expired(cache_val, key)) return std::nullopt;

    const auto epoch_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
    const std::int64_t expiry = epoch_now.count() + expires_in_seconds;
    Val updated = *cache_val;
    updated.expiry = expiry;
    cache_.insert(key, updated);
    return expires_in_seconds;
}

int64_t Database::get_expiry(const std::string& key){
    std::scoped_lock lock(cache_mutex_);
    const Val* cache_val = cache_.find(key);
    if(cache_val == nullptr) return -1;
    if(delete_if_expired(cache_val, key)) return -1;
    return cache_val->expiry.has_value()? cache_val->expiry.value():-1;
}


bool Database::delete_if_expired(const Val* cache_val, const std::string& key){
    if(cache_val->expiry.has_value()){
        if(cache_val->expiry.value()<std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()){
            //delete value 
            cache_.erase(key);
            return true;
        }
    }
    return false;
}