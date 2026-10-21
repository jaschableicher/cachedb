#include <gtest/gtest.h>
#include "database/hashtable.h"
#include "commands/value.h"
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <cstddef>   
HashTable<Value> cache_;

TEST(HashtableTests, InsertsFindsAndDeletesValue){
    cache_.insert("hello", "world");
    EXPECT_TRUE(cache_.erase("hello"));
    EXPECT_FALSE(cache_.erase("hello"));
}

TEST(HashtableTests, HandlesBinaryData){
    Bytes insert_bytes = {std::byte{0x05}, std::byte{0x08}, std::byte{0x02}};
    cache_.insert("byte", insert_bytes);
    EXPECT_TRUE(cache_.erase("byte"));
    EXPECT_FALSE(cache_.erase("byte"));
}