#include "database.h"



Database::Database(): running_(true){
    //start the expiration thread
    expiration_thread=std::thread(&Database::active_expiration_check,this);        
};

Database::~Database(){
    running_.store(false);
    expiration_condition_.notify_one();
    expiration_thread.join();
}



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
