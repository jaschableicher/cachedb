#include <gtest/gtest.h>
#include "src/commands/parser.h"


TEST(CommandTests,parse_set_command){
    Command correct_command = parse_command("SET haja bubu");
    EXPECT_EQ(correct_command.name, "SET");
    EXPECT_EQ(correct_command.args.size(),2);
    EXPECT_EQ(std::get<std::string>(correct_command.args[0]), "haja");
    EXPECT_EQ(std::get<std::string>(correct_command.args[1]), "bubu");
}

TEST(CommandTests,parse_get_command){
    Command correct_command = parse_command("GET coolKey");
    EXPECT_EQ(correct_command.name, "GET");
    EXPECT_EQ(correct_command.args.size(),1);
    EXPECT_EQ(std::get<std::string>(correct_command.args[0]), "coolKey");
}

TEST(CommandTests,parse_del_command){
    Command correct_command = parse_command("DEL asv");
    EXPECT_EQ(correct_command.name, "DEL");
    EXPECT_EQ(correct_command.args.size(),1);
    EXPECT_EQ(std::get<std::string>(correct_command.args[0]), "asv");
}

TEST(CommandTests,parse_unknown_command){
    Command unknown_command = parse_command("wfq haja bubu");
    EXPECT_EQ(unknown_command.name, "wfq");
    EXPECT_EQ(unknown_command.args.size(),2);
    EXPECT_EQ(std::get<std::string>(unknown_command.args[0]), "haja");
    EXPECT_EQ(std::get<std::string>(unknown_command.args[1]), "bubu");
}

TEST(CommandTests,parse_no_args){
    Command emptyArgs = parse_command("SET");
    EXPECT_EQ(emptyArgs.name, "SET");
    EXPECT_EQ(emptyArgs.args.size(),0);
}

TEST(CommandTests, ignores_trailing_newline) {
    Command command = parse_command("SET hello world\n");
    EXPECT_EQ(command.name, "SET");
    ASSERT_EQ(command.args.size(), 2);
    EXPECT_EQ(std::get<std::string>(command.args[0]), "hello");
    EXPECT_EQ(std::get<std::string>(command.args[1]), "world");
}


TEST(CommandTests, parses_bool_arguments) {
    Command command = parse_command("SET flags true false");

    ASSERT_EQ(command.args.size(), 3);
    ASSERT_TRUE(std::holds_alternative<bool>(command.args[1]));
    ASSERT_TRUE(std::holds_alternative<bool>(command.args[2]));
    EXPECT_TRUE(std::get<bool>(command.args[1]));
    EXPECT_FALSE(std::get<bool>(command.args[2]));
}

TEST(CommandTests, parses_int_argument) {
    Command command = parse_command("SET offset -42");

    ASSERT_EQ(command.args.size(), 2);
    ASSERT_TRUE(std::holds_alternative<int64_t>(command.args[1]));
    EXPECT_EQ(std::get<int64_t>(command.args[1]), -42);
}

TEST(CommandTests, parses_double_argument) {
    Command command = parse_command("SET ratio 3.14159");

    ASSERT_EQ(command.args.size(), 2);
    ASSERT_TRUE(std::holds_alternative<double>(command.args[1]));
    EXPECT_DOUBLE_EQ(std::get<double>(command.args[1]), 3.14159);
}
