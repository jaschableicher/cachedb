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
    const auto* str_ptr = cache_.find("hello");
    ASSERT_NE(str_ptr, nullptr);
    EXPECT_EQ(std::get<std::string>(*str_ptr), "world");
    EXPECT_TRUE(cache_.erase("hello"));
}

TEST(HashtableTests, HandlesBinaryData){
    Bytes insert_bytes = {std::byte{0x05}, std::byte{0x08}, std::byte{0x02}};
    cache_.insert("byte", insert_bytes);

    auto* val = cache_.find("byte");
    ASSERT_NE(val, nullptr);
    
    // std::get_if safely checks the variant type
    auto* bytes = std::get_if<Bytes>(val);
    ASSERT_NE(bytes, nullptr);
    EXPECT_EQ(*bytes, insert_bytes);
    cache_.erase("byte");
}