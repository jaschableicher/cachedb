#include <gtest/gtest.h>
#include "database/database.h"

TEST(DatabaseTest, SetAndGet) {
    Database db;

    db.set("name", "alice");

    const auto value = db.get("name");
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(*value));
    EXPECT_EQ(std::get<std::string>(*value), "alice");
}

TEST(DatabaseTest, SetOverwritesValue) {
    Database db;
    db.set("x", "1");
    db.set("x", "2");

    const auto value = db.get("x");
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(*value));
    EXPECT_EQ(std::get<std::string>(*value), "2");
}

TEST(DatabaseTest, MissingKeyReturnsNullopt) {
    Database db;

    EXPECT_FALSE(db.get("missing").has_value());
}

TEST(DatabaseTest, UnexpiredKeyRemainsAvailable) {
    Database db;
    db.set("key", "value");

    EXPECT_EQ(db.set_expiry("key", 60), 60);
    EXPECT_TRUE(db.get("key").has_value());
}

TEST(DatabaseTest, ExpiredKeyIsNotReturned) {
    Database db;
    db.set("key", "value");

    EXPECT_EQ(db.set_expiry("key", 0), 0);
    EXPECT_FALSE(db.get("key").has_value());
}
/*
TEST(DatabaseTest, HandlesBinaryData) {
    Database db;

    std::vector<std::byte> binary_data = {
        std::byte{0x00}, std::byte{0x01}, std::byte{0x7f}, std::byte{0xff}
    };

    std::string binary_value;
    binary_value.reserve(binary_data.size());
    for (const auto byte : binary_data) {
        binary_value.push_back(static_cast<char>(byte));
    }

    db.set("binary", binary_value);

    const auto value = db.get("binary");
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(*value));
    EXPECT_EQ(std::get<std::string>(*value), binary_value);
}*/
