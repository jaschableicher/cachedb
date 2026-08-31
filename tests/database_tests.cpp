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