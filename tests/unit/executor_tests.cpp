#include <gtest/gtest.h>
#include "database/database.h"
#include "commands/executor.h"
#include "commands/command.h"

TEST(ExecutorTest, SetThenGet) {
    register_commands();
    Database db;

    EXPECT_EQ(
        execute_command(db, "SET language cpp"),
        "OK"
    );

    EXPECT_EQ(
        execute_command(db, "GET language"),
        "cpp"
    );
}
