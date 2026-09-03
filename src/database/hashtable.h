#ifndef HASHTABLE_H
#define HASHTABLE_H
#include "value.h"
#include <iostream>
#include <list>
template<typename V>
class HashTable {
private:
    struct Entry {
        std::string key;
        V value;
    };

    std::vector<std::list<Entry>> buckets_;
    std::size_t index(const std::string& key) const{
        return std::hash<std::string>{}(key) % buckets_.size();
    }
public:
    explicit HashTable(std::size_t bucket_count = 16) : buckets_(std::max<std::size_t>(bucket_count, 1)){}
    
    void insert(const std::string& key, const V& value){
        auto& bucket = buckets_[index(key)];

        for(auto& entry : bucket){
            if(entry.key==key){
                entry.value=value;
                return;
            }
        }

        bucket.push_back({key, value});
    }

    const V* find(const std::string&key) const{
        const auto& bucket = buckets_[index(key)];
        for(const auto& entry : bucket){
            if(entry.key == key){
                return &entry.value;
            }
        }
        return nullptr;
    }

    bool erase(const std::string& key){
        auto& bucket = buckets_[index(key)];
        for(auto it = bucket.begin();it != bucket.end();++it){
            if(it->key == key){
                bucket.erase(it);
                return true;
            }
        }
        return false;
    }
};
#endif