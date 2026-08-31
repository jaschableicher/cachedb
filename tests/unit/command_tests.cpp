#include <gtest/gtest.h>
#include "src/commands/parser.h"


TEST(CommandTests,parse_set_command){
    Command correct_command = parse_command("SET haja bubu");
    EXPECT_EQ(correct_command.type, CommandType::Set);
    EXPECT_EQ(correct_command.args.size(),2);
    EXPECT_EQ(correct_command.args[0],"haja");
    EXPECT_EQ(correct_command.args[1],"bubu");  
}

TEST(CommandTests,parse_get_command){
    Command correct_command = parse_command("GET coolKey");
    EXPECT_EQ(correct_command.type, CommandType::Get);
    EXPECT_EQ(correct_command.args.size(),1);
    EXPECT_EQ(correct_command.args[0],"coolKey");
}

TEST(CommandTests,parse_del_command){
    Command correct_command = parse_command("DEL asv");
    EXPECT_EQ(correct_command.type, CommandType::Del);
    EXPECT_EQ(correct_command.args.size(),1);
    EXPECT_EQ(correct_command.args[0],"asv");
}

TEST(CommandTests,parse_unknown_command){
    Command unknown_command = parse_command("wfq haja bubu");
    EXPECT_EQ(unknown_command.type, CommandType::Unknown);
    EXPECT_EQ(unknown_command.args.size(),2);
    EXPECT_EQ(unknown_command.args[0],"haja");
    EXPECT_EQ(unknown_command.args[1],"bubu");
}

TEST(CommandTests,parse_no_args){
    Command emptyArgs = parse_command("SET");
    EXPECT_EQ(emptyArgs.type, CommandType::Set);
    EXPECT_EQ(emptyArgs.args.size(),0);
}