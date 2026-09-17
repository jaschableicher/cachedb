#include <gtest/gtest.h>
#include "src/commands/parser.h"

namespace {
Command parse_test_command(const std::string& name, const std::vector<Value>& args) {
    msgpack::sbuffer buffer;
    msgpack::packer<msgpack::sbuffer> writer(buffer);
    writer.pack_array(2);
    writer.pack(name);
    writer.pack_array(static_cast<uint32_t>(args.size()));

    for (const auto& arg : args) {
        auto encoded = protocol::encode_value(arg);
        auto value = msgpack::unpack(encoded.data(), encoded.size());
        writer.pack(value.get());
    }

    return parse_command(buffer.data(), buffer.size());
}
}

TEST(CommandTests,parse_set_command){
    Command correct_command = parse_test_command("SET", {std::string("haja"), std::string("bubu")});
    EXPECT_EQ(correct_command.name, "SET");
    EXPECT_EQ(correct_command.args.size(),2);
    EXPECT_EQ(std::get<std::string>(correct_command.args[0]), "haja");
    EXPECT_EQ(std::get<std::string>(correct_command.args[1]), "bubu");
}

TEST(CommandTests,parse_get_command){
    Command correct_command = parse_test_command("GET", {std::string("coolKey")});
    EXPECT_EQ(correct_command.name, "GET");
    EXPECT_EQ(correct_command.args.size(),1);
    EXPECT_EQ(std::get<std::string>(correct_command.args[0]), "coolKey");
}

TEST(CommandTests,parse_del_command){
    Command correct_command = parse_test_command("DEL", {std::string("asv")});
    EXPECT_EQ(correct_command.name, "DEL");
    EXPECT_EQ(correct_command.args.size(),1);
    EXPECT_EQ(std::get<std::string>(correct_command.args[0]), "asv");
}

TEST(CommandTests,parse_unknown_command){
    Command unknown_command = parse_test_command("wfq", {std::string("haja"), std::string("bubu")});
    EXPECT_EQ(unknown_command.name, "wfq");
    EXPECT_EQ(unknown_command.args.size(),2);
    EXPECT_EQ(std::get<std::string>(unknown_command.args[0]), "haja");
    EXPECT_EQ(std::get<std::string>(unknown_command.args[1]), "bubu");
}

TEST(CommandTests,parse_no_args){
    Command emptyArgs = parse_test_command("SET", {});
    EXPECT_EQ(emptyArgs.name, "SET");
    EXPECT_EQ(emptyArgs.args.size(),0);
}

TEST(CommandTests, ignores_trailing_newline) {
    Command command = parse_test_command("SET", {std::string("hello"), std::string("world")});
    EXPECT_EQ(command.name, "SET");
    ASSERT_EQ(command.args.size(), 2);
    EXPECT_EQ(std::get<std::string>(command.args[0]), "hello");
    EXPECT_EQ(std::get<std::string>(command.args[1]), "world");
}


TEST(CommandTests, parses_bool_arguments) {
    Command command = parse_test_command("SET", {std::string("flags"), true, false});

    ASSERT_EQ(command.args.size(), 3);
    ASSERT_TRUE(std::holds_alternative<bool>(command.args[1]));
    ASSERT_TRUE(std::holds_alternative<bool>(command.args[2]));
    EXPECT_TRUE(std::get<bool>(command.args[1]));
    EXPECT_FALSE(std::get<bool>(command.args[2]));
}

TEST(CommandTests, parses_int_argument) {
    Command command = parse_test_command("SET", {std::string("offset"), int64_t{-42}});

    ASSERT_EQ(command.args.size(), 2);
    ASSERT_TRUE(std::holds_alternative<int64_t>(command.args[1]));
    EXPECT_EQ(std::get<int64_t>(command.args[1]), -42);
}

TEST(CommandTests, parses_double_argument) {
    Command command = parse_test_command("SET", {std::string("ratio"), 3.14159});

    ASSERT_EQ(command.args.size(), 2);
    ASSERT_TRUE(std::holds_alternative<double>(command.args[1]));
    EXPECT_DOUBLE_EQ(std::get<double>(command.args[1]), 3.14159);
}
