#include <gtest/gtest.h>
#include "database/database.h"

TEST(DatabaseTest, SetAndGet) {
    Database db;

    db.set("name", "alice");

    ASSERT_TRUE(db.get("name").has_value());
    EXPECT_EQ(db.get("name").value(), "alice");
}

TEST(DatabaseTest, SetOverwritesValue) {
    Database db;
    db.set("x", "1");
    db.set("x", "2");
    EXPECT_EQ(db.get("x").value(), "2");
}

TEST(DatabaseTest, MissingKeyReturnsNullopt) {
    Database db;

    EXPECT_FALSE(db.get("missing").has_value());
}

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

    ASSERT_TRUE(db.get("binary").has_value());
    EXPECT_EQ(db.get("binary").value(), binary_value);
}