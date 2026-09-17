#include <gtest/gtest.h>
#include "database/database.h"
#include "commands/executor.h"
#include "commands/command.h"

namespace {
void ensure_commands_registered() {
    static const int initialized = (register_commands(), 0);
    (void)initialized;
}
}

TEST(ExecutorTest, SetThenGet) {
    ensure_commands_registered();
    Database db;

    EXPECT_EQ(
        std::get<std::string>(execute_command(db, Command{.name = "SET", .args = {std::string("language"), std::string("cpp")}})),
        "OK"
    );

    EXPECT_EQ(
        std::get<std::string>(execute_command(db, Command{.name = "GET", .args = {std::string("language")}})),
        "cpp"
    );
}

TEST(ExecutorTest, ExpireAcceptsIntegerText) {
    ensure_commands_registered();
    Database db;

    ASSERT_EQ(std::get<std::string>(execute_command(db, Command{.name = "SET", .args = {std::string("temporary"), std::string("value")}})), "OK");
    EXPECT_EQ(std::get<int64_t>(execute_command(db, Command{.name = "EXPIRE", .args = {std::string("temporary"), int64_t{10}}})), 10);
    EXPECT_EQ(std::get<std::string>(execute_command(db, Command{.name = "GET", .args = {std::string("temporary")}})), "value");
}

TEST(ExecutorTest, ExpireRejectsNonIntegerText) {
    ensure_commands_registered();
    Database db;

    ASSERT_EQ(std::get<std::string>(execute_command(db, Command{.name = "SET", .args = {std::string("temporary"), std::string("value")}})), "OK");
    EXPECT_EQ(
        std::get<std::string>(execute_command(db, Command{.name = "EXPIRE", .args = {std::string("temporary"), std::string("10seconds")}})),
        "ERR invalid argument type at index 1"
    );
}

TEST(ExecutorTest, SetThenGetPreservesBinaryData) {
    ensure_commands_registered();
    Database db;
    CommandContext context{db};
    Bytes payload{
        std::byte{0x00},
        std::byte{0x01},
        std::byte{0x7F},
        std::byte{0x80},
        std::byte{0xFF}
    };

    ExecuteReturn set_result = Registry::get_instance()->execute(
        context,
        Command{.name = "SET", .args = {std::string("binary"), payload}}
    );

    ASSERT_TRUE(std::holds_alternative<std::string>(set_result.value));
    EXPECT_EQ(std::get<std::string>(set_result.value), "OK");

    ExecuteReturn get_result = Registry::get_instance()->execute(
        context,
        Command{.name = "GET", .args = {std::string("binary")}}
    );

    ASSERT_TRUE(std::holds_alternative<Bytes>(get_result.value));
    EXPECT_EQ(std::get<Bytes>(get_result.value), payload);
}

TEST(ExecutorTest, LargeData) {
    ensure_commands_registered();
    Database db;
    const std::string value = "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.  Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet. Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea takimata sanctus est Lorem ipsum dolor sit amet.";

    EXPECT_EQ(
        std::get<std::string>(execute_command(db, Command{.name = "SET", .args = {std::string("test"), value}})),
        "OK"
    );

    EXPECT_EQ(
        std::get<std::string>(execute_command(db, Command{.name = "GET", .args = {std::string("test")}})),
        value
    );
}
