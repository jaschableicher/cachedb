#include "database.h"
#include <iostream>
#include "logger/logger.h"

void Database::active_expiration_check(){
    while(running_.load()){
        {
            std::unique_lock lock(expiration_mutex_);
            if (!running_.load()) break;

            if (expiration_heap_.empty()) {
                expiration_condition_.wait(lock, [this] {
                    return !running_.load() || !expiration_heap_.empty();
                });
            } else {
                const auto next_expiry = std::chrono::system_clock::time_point(
                    std::chrono::seconds(expiration_heap_.top()->expiry)
                );
                expiration_condition_.wait_until(lock, next_expiry);
            }
        }

        if (!running_.load()) break;

        int max_checks = 0;
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::scoped_lock lock(expiration_mutex_);
        while(!expiration_heap_.empty() && expiration_heap_.top()->expiry <= now && max_checks < 100){
            max_checks++;
            auto* item = expiration_heap_.top();
            const std::string key = item->key;
            expiration_heap_.pop();

            cache_.erase(key);
            Command cmd={
                "DEL",
                {key}
            };
            Logger::get_instance()->log_command(cmd);
            expiration_entries_.erase(key);
        }
        
    }
}

std::optional<std::int64_t> Database::set_expiry(const std::string& key,int64_t expires_in_seconds) {
    {
        std::scoped_lock lock(expiration_mutex_);
        Val cache_val;
        auto getter = [&](Val entry_data) {
            cache_val = entry_data;
        };
        if (!cache_.with_read(key, getter)) return std::nullopt;
        if (delete_if_expired(&cache_val, key)) return std::nullopt;

        const auto epoch_now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
        const std::int64_t expiry = epoch_now.count() + expires_in_seconds;
        Val updated = cache_val;
        updated.expiry = expiry;
        cache_.insert(key, updated);
        auto [it, inserted] = expiration_entries_.try_emplace(key, ExpirationEntry{key, expiry});
        if (inserted) expiration_heap_.insert(it->second);
        else expiration_heap_.update(it->second, expiry);
    }

    expiration_condition_.notify_one();
    return expires_in_seconds;
}

int64_t Database::get_expiry(const std::string& key){
    std::scoped_lock lock(expiration_mutex_);
    Val cache_val;
    auto getter = [&](Val entry_data){
        cache_val=entry_data;
    };
    
    bool cache_exists = cache_.with_read(key,getter);
    if(!cache_exists) return -1;
    if(delete_if_expired(&cache_val, key)) return -1;
    return cache_val.expiry.has_value()? cache_val.expiry.value():-1;
}


bool Database::delete_if_expired(const Val* cache_val, const std::string& key){
    if(cache_val->expiry.has_value()){
        if(cache_val->expiry.value() <= std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()){
            //delete value 
            cache_.erase(key);
            if (auto it = expiration_entries_.find(key); it != expiration_entries_.end()) {
                expiration_heap_.remove(it->second);
                expiration_entries_.erase(it);
            }
            Command cmd={
                "DEL",
                {key}
            };
            Logger::get_instance()->log_command(cmd);
            return true;
        }
    }
    return false;
}
