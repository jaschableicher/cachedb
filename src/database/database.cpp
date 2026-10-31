#include "database.h"



Database::Database(): running_(true){
    //start the expiration thread
    expiration_thread=std::thread(&Database::active_expiration_check,this);        
};

Database::~Database(){
    {
        std::scoped_lock lock(expiration_mutex_);
        running_.store(false);
    }
    expiration_condition_.notify_one();
    expiration_thread.join();
}



void Database::set(std::string key, const Value& value) {
    std::scoped_lock lock(expiration_mutex_);
    if (auto it = expiration_entries_.find(key); it != expiration_entries_.end()) {
        expiration_heap_.remove(it->second);
        expiration_entries_.erase(it);
    }
    cache_.insert(key, Val{value, std::nullopt});
}

std::optional<Value> Database::get(const std::string& key){
    Val cache_val;
    auto getter = [&](Val entry_data){
        cache_val=entry_data;
    };
    if (!cache_.with_read(key, getter)) return std::nullopt;
    if (!cache_val.expiry.has_value()) return cache_val.data;

    std::scoped_lock lock(expiration_mutex_);
    if (!cache_.with_read(key, getter)) return std::nullopt;
    if(delete_if_expired(&cache_val, key)) return std::nullopt;
    return cache_val.data;
}

bool Database::erase(const std::string& key){
    std::scoped_lock lock(expiration_mutex_);
    if (auto it = expiration_entries_.find(key); it != expiration_entries_.end()) {
        expiration_heap_.remove(it->second);
        expiration_entries_.erase(it);
    }
    return cache_.erase(key);
}
