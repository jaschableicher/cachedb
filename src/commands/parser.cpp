#include "parser.h"

std::vector<Value> parse_arguments(std::string_view input) {
    std::vector<Value> args;

    for (std::size_t i = 0; i < input.size();) {
        while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i]))) ++i;
        if (i >= input.size()) break;

        std::string arg;

        if (std::string_view("\"'").find(input[i]) != std::string_view::npos) {
            char quotation = input[i];
            ++i;

            while (i < input.size() && input[i] != quotation) {
                if (input[i] == '\\' && i + 1 < input.size()) {
                    i++;

                    switch (input[i]) {
                        case 'n': arg += '\n'; break;
                        case 't': arg += '\t'; break;
                        case '"': arg += '"'; break;
                        case '\'':   arg += '\''; break;
                        case '\\': arg += '\\'; break;
                        default: arg += input[i]; break;
                    }
                } else {
                    arg += input[i];
                }

                i++;
            }

            if (i < input.size() && input[i] == quotation) ++i;
        } else {
            while (i < input.size() && !std::isspace(static_cast<unsigned char>(input[i]))) {
                arg += input[i++];
            }
        }

        args.push_back(std::move(arg));
    }

    return args;
}

Command parse_command(std::string_view input){

    Command command;

    //get the first word, which must be GET SET etc or Unknown if it is wrong
    const auto first = input.find_first_not_of(" \t\n\r\f\v"); //Killa function, starts whenever there is the first actual character
    if (first == std::string_view::npos) return {}; //If this is the end return

    const auto last = input.find_first_of(" \t\n\r\f\v",first);//Looks for end of word
    std::string_view name  = input.substr(first,last==std::string_view::npos ? std::string_view::npos : last - first);

    command.name = std::string(name);

    //Now if existent, push the rest simply on there as the rest of the vector
    if(last==std::string_view::npos){
        return command; //Return with no arguments
    }

    std::string_view rest_of_input = input.substr(last);
    //Loop through the rest of the input string and push the arguments on there
    command.args=std::move(parse_arguments(rest_of_input));

    return command;

}
