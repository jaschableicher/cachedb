#include "database.h"
# define MAX_MEMORY_ALLOWED 123455123//Test value


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

constexpr size_t overhead=sizeof(uint64_t)+sizeof(uint8_t);


SetResult Database::set(std::string key, const Value& value) {
    //check memory first!
    size_t estimated_cost = sizeof(value)+sizeof(key)+overhead;
    
    if(accounted_memory+estimated_cost>MAX_MEMORY_ALLOWED) return SetResult::MemoryLimitExceeded;
    accounted_memory+=estimated_cost;
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
    auto val = this->get(key);
    accounted_memory-=overhead+sizeof(val.has_value()?val.value():0);
    return cache_.erase(key);
}
