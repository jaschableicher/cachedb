#include "parser.h"


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
    std::size_t argument_start = rest_of_input.find_first_not_of(" \t\n\r\f\v");
    while(argument_start != std::string_view::npos){
        const auto argument_end = rest_of_input.find_first_of(" \t\n\r\f\v", argument_start);
        command.args.emplace_back(rest_of_input.substr(
            argument_start,
            argument_end == std::string_view::npos
                ? std::string_view::npos
                : argument_end - argument_start));

        if(argument_end == std::string_view::npos) break;
        argument_start = rest_of_input.find_first_not_of(" \t\n\r\f\v", argument_end);
    }

    return command;

}