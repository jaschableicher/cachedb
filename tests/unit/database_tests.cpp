#include <gtest/gtest.h>
#include "database/database.h"

TEST(DatabaseTest, RepeatedShutdownWithNoExpirations) {
    for (int i = 0; i < 1000; ++i) {
        Database db;
        std::this_thread::yield();
    }
}

TEST(DatabaseTest, RepeatedShutdownWithPendingExpiration) {
    for (int i = 0; i < 1000; ++i) {
        Database db;
        db.set("key", "value");
        ASSERT_EQ(db.set_expiry("key", 3600), 3600);
        std::this_thread::yield();
    }
}

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

TEST(DatabaseTest, OverwriteAndEraseRemoveExpiration) {
    Database db;
    db.set("overwritten", "old");
    ASSERT_EQ(db.set_expiry("overwritten", 3600), 3600);
    db.set("overwritten", "new");
    EXPECT_EQ(db.get_expiry("overwritten"), -1);
    const auto overwritten = db.get("overwritten");
    ASSERT_TRUE(overwritten.has_value());
    EXPECT_EQ(std::get<std::string>(*overwritten), "new");

    db.set("deleted", "value");
    ASSERT_EQ(db.set_expiry("deleted", 3600), 3600);
    EXPECT_TRUE(db.erase("deleted"));
    EXPECT_FALSE(db.get("deleted").has_value());
    db.set("deleted", "replacement");
    EXPECT_EQ(db.get_expiry("deleted"), -1);
}

TEST(DatabaseTest, RepeatedExpiryUpdateUsesLatestDeadline) {
    Database db;
    db.set("key", "value");
    ASSERT_EQ(db.set_expiry("key", 3600), 3600);
    ASSERT_EQ(db.set_expiry("key", 0), 0);
    EXPECT_FALSE(db.get("key").has_value());
}

TEST(DatabaseTest, UpdatingExpirationReordersHeap) {
    ExpirationHeap heap;
    ExpirationEntry first{"first", 10};
    ExpirationEntry second{"second", 20};
    ExpirationEntry third{"third", 30};
    heap.insert(first);
    heap.insert(second);
    heap.insert(third);

    heap.update(third, 5);
    EXPECT_EQ(heap.top(), &third);
    heap.update(third, 40);
    EXPECT_EQ(heap.top(), &first);
    heap.remove(first);
    EXPECT_EQ(heap.top(), &second);
    heap.pop();
    EXPECT_EQ(heap.top(), &third);
    heap.pop();
    EXPECT_TRUE(heap.empty());
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
