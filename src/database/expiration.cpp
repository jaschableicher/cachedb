#include "database.h"
#include <iostream>


void Database::active_expiration_check(){
    while(running_.load()){
        //Itterate only over the first 20 elements not more for now incase there are thousands of expiration key the minheap will handle it!
        int max_checks = 0;
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::scoped_lock lock(expiration_mutex_,cache_mutex_);
        while(!expiration_heap_.empty()&& expiration_heap_.top().expiry<now && max_checks<100){
            max_checks++;
            ExpirationEntry item = expiration_heap_.top();
            expiration_heap_.pop();
            //check version with hashmap
            const Val* cached_val = cache_.find(item.key);
            if(cached_val!=nullptr && cached_val->version==item.version){
   
                cache_.erase(item.key);
            }
        }
        
    }
}

std::optional<std::int64_t> Database::set_expiry(const std::string& key,int64_t expires_in_seconds) {
    
    std::scoped_lock lock(cache_mutex_, expiration_mutex_);
    const Val* cache_val = cache_.find(key);
    if (cache_val == nullptr) return std::nullopt;
    if(delete_if_expired(cache_val, key)) return std::nullopt;

    const auto epoch_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
    const std::int64_t expiry = epoch_now.count() + expires_in_seconds;
    Val updated = *cache_val;
    updated.expiry = expiry;
    updated.version +=1;
    cache_.insert(key, updated);
    expiration_heap_.emplace(ExpirationEntry{
        key,
        expiry,
        updated.version
    });
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
